// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — GLTimer implementation
// Ported from: itwinjs-core core/frontend/src/internal/render/webgl/GLTimer.ts
#include "GLTimer.h"

#include "rhi/opengl/GlLoader.h"   // zogl* 查询函数指针（glGenQueries/glBeginQuery/...）

#include <cassert>

BEGIN_DQ_RENDER_NAMESPACE

// ---------------------------------------------------------------------------
// GLTimerQueryExtension — the production adapter (GL_TIME_ELAPSED query
// objects; desktop-GL equivalent of EXT_disjoint_timer_query, GLTimer.ts:11-38)
// ---------------------------------------------------------------------------
namespace {

class GLTimerQueryExtension : public TimerQueryExtension {
public:
    bool isSupported() const override
    {
        return zoglGenQueries != nullptr && zoglDeleteQueries != nullptr
            && zoglBeginQuery != nullptr && zoglEndQuery != nullptr
            && zoglGetQueryObjectuiv != nullptr && zoglGetQueryObjectui64v != nullptr;
    }

    uint32_t createQuery() override
    {
        uint32_t q = 0;
        glGenQueries(1, &q);
        return q;
    }
    void deleteQuery(uint32_t q) override
    {
        if (q != 0)
            glDeleteQueries(1, &q);
    }
    void beginQuery(uint32_t q) override { glBeginQuery(GL_TIME_ELAPSED, q); }
    void endQuery() override { glEndQuery(GL_TIME_ELAPSED); }
    bool isResultAvailable(uint32_t q) const override
    {
        uint32_t available = GL_FALSE;
        glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &available);
        return available == GL_TRUE;
    }
    uint64_t getResult(uint32_t q) const override
    {
        uint64_t ns = 0;
        glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
        return ns;
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// GLTimer
// ---------------------------------------------------------------------------
GLTimer::GLTimer()
    : m_extension(std::make_unique<GLTimerQueryExtension>())
{
    m_ctrl.isGLTimerSupported = isSupported();
}

GLTimer::GLTimer(std::unique_ptr<TimerQueryExtension> extension)
    : m_extension(std::move(extension))
{
    m_ctrl.isGLTimerSupported = isSupported();
}

// GLTimer.ts:78 isSupported.
bool GLTimer::isSupported() const noexcept
{
    return m_extension && m_extension->isSupported();
}

// GLTimer.ts:87-92 beginOperation(label).
void GLTimer::beginOperation(std::string const& label)
{
    if (!m_ctrl.resultsCallback || !isSupported())
        return;
    if (m_queryStack.empty())
        return;  // 参考的不变量：operation 只在帧内发起（beginFrame 先行）
    pushQuery(label);
}

// GLTimer.ts:94-101 endOperation — reference throws on mismatch
// (IModelError "Mismatched calls"); DanQing core engine 禁异常 → debug 断言 +
// release 静默返回（§9 错误策略）。
void GLTimer::endOperation()
{
    if (!m_ctrl.resultsCallback || !isSupported())
        return;
    assert(!m_queryStack.empty() && "Mismatched calls to beginOperation/endOperation");
    if (m_queryStack.empty())
        return;
    popQuery();
}

// GLTimer.ts:103-112 beginFrame.
void GLTimer::beginFrame()
{
    if (!m_ctrl.resultsCallback || !isSupported())
        return;
    assert(m_queryStack.empty() && "Already recording timing for a frame");
    if (!m_queryStack.empty())
        return;

    auto query = m_extension->createQuery();
    m_extension->beginQuery(query);
    m_frameRoot = std::make_unique<QueryEntry>();
    m_frameRoot->label = "Total";
    m_frameRoot->query = query;
    m_queryStack.push_back(m_frameRoot.get());
}

// GLTimer.ts:114-172 endFrame — pop the frame root into the delivery queue,
// then poll pending results (the reference's setTimeout-retry equivalent).
void GLTimer::endFrame()
{
    if (!m_ctrl.resultsCallback || !isSupported())
        return;
    assert(m_queryStack.size() == 1 && "Missing at least one endOperation call");
    if (m_queryStack.size() != 1)
        return;

    m_extension->endQuery();
    m_queryStack.pop_back();
    m_pendingResults.push_back(std::move(m_frameRoot));

    pollPendingResults();
}

// GLTimer.ts:126-171 queryCallback — deliver the oldest pending frame when
// its root (last-completed) query is available. didDisjointEventHappen() is
// always false on desktop GL (see the header note).
void GLTimer::pollPendingResults()
{
    while (!m_pendingResults.empty()) {
        auto& root = *m_pendingResults.front();
        // Check only the root (last query completed); with siblings, the last one.
        uint32_t const finalQuery = root.siblingQueries.empty()
                                        ? root.query
                                        : root.siblingQueries.back();
        if (!m_extension->isResultAvailable(finalQuery))
            return;  // results arrive in order; wait for the oldest

        if (m_extension->didDisjointEventHappen()) {
            cleanupAfterDisjointEvent(root);
            m_pendingResults.pop_front();
            continue;
        }

        auto userCallback = m_ctrl.resultsCallback;
        GLTimerResult result = processQueryEntry(root);
        m_pendingResults.pop_front();
        if (userCallback)
            userCallback(result);
    }
}

// GLTimer.ts:142-167 processQueryEntry — read ns, delete the query, fold
// sibling times in, recurse into children (inclusive times).
GLTimerResult GLTimer::processQueryEntry(QueryEntry& entry)
{
    uint64_t const time = m_extension->getResult(entry.query);
    m_extension->deleteQuery(entry.query);

    GLTimerResult result;
    result.label = entry.label;
    result.nanoseconds = time;

    for (uint32_t sib : entry.siblingQueries) {
        result.nanoseconds += m_extension->getResult(sib);
        m_extension->deleteQuery(sib);
    }
    entry.siblingQueries.clear();

    for (auto& child : entry.children) {
        GLTimerResult childResult = processQueryEntry(*child);
        result.nanoseconds += childResult.nanoseconds;
        result.children.push_back(std::move(childResult));
    }
    return result;
}

// GLTimer.ts:174-185 cleanupAfterDisjointEvent.
void GLTimer::cleanupAfterDisjointEvent(QueryEntry& entry)
{
    m_extension->deleteQuery(entry.query);
    for (uint32_t sib : entry.siblingQueries)
        m_extension->deleteQuery(sib);
    entry.siblingQueries.clear();
    for (auto& child : entry.children)
        cleanupAfterDisjointEvent(*child);
}

// GLTimer.ts:187-201 pushQuery — end the active query, start a child query;
// the entry lives in the active parent's children (owning) AND on the stack
// (non-owning, until popQuery).
void GLTimer::pushQuery(std::string const& label)
{
    m_extension->endQuery();

    auto query = m_extension->createQuery();
    m_extension->beginQuery(query);

    auto& activeQuery = *m_queryStack.back();
    auto queryEntry = std::make_unique<QueryEntry>();
    queryEntry->label = label;
    queryEntry->query = query;
    QueryEntry* raw = queryEntry.get();
    activeQuery.children.push_back(std::move(queryEntry));
    m_queryStack.push_back(raw);
}

// GLTimer.ts:203-214 popQuery — end the child, resume the parent as a sibling.
void GLTimer::popQuery()
{
    m_extension->endQuery();
    m_queryStack.pop_back();

    auto& activeQuery = *m_queryStack.back();
    auto newQuery = m_extension->createQuery();
    activeQuery.siblingQueries.push_back(newQuery);
    m_extension->beginQuery(newQuery);
}

END_DQ_RENDER_NAMESPACE
