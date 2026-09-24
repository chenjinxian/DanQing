// SPDX-License-Identifier: Apache-2.0
// Ported from: imodel-native iModelCore/Bentley/PublicAPI/Bentley/btree/btree_map.h
//   Original upstream: Google cpp-btree (Kronuz fork), (C) 2019 German Mendez Bravo,
//   (C) 2013 Google Inc. Apache License 2.0. See btree.h for full provenance.
//   Identifier rename + include repoint as in btree.h. CLAUDE.md §7.3: do not replace with std::map.
/*---------------------------------------------------------------------------------------------
* Copyright (c) Bentley Systems, Incorporated. All rights reserved.
* See LICENSE.md in the repository root for full copyright notice.
*--------------------------------------------------------------------------------------------*/
#pragma once

/*
 * Copyright (c) 2019 German Mendez Bravo (Kronuz)
 * Copyright (c) 2013 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * A btree::map<> implements the STL unique sorted associative container
 * interface and the pair associative container interface (a.k.a map<>) using a
 * btree. See btree.h for details of the btree implementation and caveats.
 */

#include "btree.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

// vendored btree 移植：抑制 user-provided copy ctor + 隐式 copy assign 的 C++20 告警（同 btree.h）。
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-copy-with-user-provided-copy"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy-with-user-provided-copy"
#endif

BEGIN_DQ_BASE_NAMESPACE

// A common base class for map and safe_map.
template <typename Tree>
class btree_map_container : public btree_unique_container<Tree> {
	typedef btree_map_container<Tree> self_type;
	typedef btree_unique_container<Tree> super_type;

public:
	typedef typename Tree::key_type key_type;
	typedef typename Tree::data_type data_type;
	typedef typename Tree::value_type value_type;
	typedef typename Tree::mapped_type mapped_type;
	typedef typename Tree::key_compare key_compare;
	typedef typename Tree::allocator_type allocator_type;
	typedef typename Tree::iterator iterator;
	typedef typename Tree::const_iterator const_iterator;

public:
	// Default constructor.
	btree_map_container(const key_compare& comp = key_compare(),
				  const allocator_type& alloc = allocator_type())
			: super_type(comp, alloc) {
	}

	// Copy constructor.
	btree_map_container(const self_type& x)
			: super_type(x) {
	}

	// Range constructor.
	template <class InputIterator>
	btree_map_container(InputIterator b, InputIterator e,
				  const key_compare& comp = key_compare(),
				  const allocator_type& alloc = allocator_type())
			: super_type(b, e, comp, alloc) {
	}

	template <typename... Args>
	std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args) {
		return this->tree_.emplace_unique_key_args(key,
			std::piecewise_construct,
			std::forward_as_tuple(key),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename... Args>
	std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args) {
		return this->tree_.emplace_unique_key_args(key,
			std::piecewise_construct,
			std::forward_as_tuple(std::move(key)),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename... Args>
	iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args) {
		return this->tree_.emplace_hint_unique_key_args(hint, key,
			std::piecewise_construct,
			std::forward_as_tuple(key),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	template <typename... Args>
	iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args) {
		return this->tree_.emplace_hint_unique_key_args(hint, key,
			std::piecewise_construct,
			std::forward_as_tuple(std::move(key)),
			std::forward_as_tuple(std::forward<Args>(args)...));
	}

	// Access specified element with bounds checking.
	mapped_type& at(const key_type& key) {
		auto it = this->find(key);
		if (it == this->end()) {
			// §9: 项目编译 -fno-exceptions，at() 越界为不可恢复错误。
#ifdef __EXCEPTIONS
			throw std::out_of_range("map::at:  key not found");
#else
			std::fprintf(stderr, "map::at: key not found\n");
			std::abort();
#endif
		}
		return it->second;
	}
	const mapped_type& at(const key_type& key) const {
		auto it = this->find(key);
		if (it == this->end()) {
#ifdef __EXCEPTIONS
			throw std::out_of_range("map::at:  key not found");
#else
			std::fprintf(stderr, "map::at: key not found\n");
			std::abort();
#endif
		}
		return it->second;
	}

	// Insertion routines.
	data_type& operator[](const key_type& key) {
		return this->try_emplace(key).first->second;
	}

	data_type& operator[](key_type&& key) {
		return this->try_emplace(std::move(key)).first->second;
	}
};

// The map class is needed mainly for its constructors.
template <typename Key, typename Value,
	typename Compare = std::less<Key>,
	uint16_t EntriesPerNode = 32,
	typename Alloc = dqBase::DqAllocator<std::pair<const Key, Value>>>
class bmap : public btree_map_container<
	btree<btree_map_params<Key, Value, Compare, Alloc, EntriesPerNode*sizeof(bpair<Key, Value>)>>> {

	typedef bpair<Key,Value> pair_type;
  enum { TargetNodeSize = EntriesPerNode*sizeof(pair_type)};

	typedef bmap<Key, Value, Compare, EntriesPerNode, Alloc> self_type;
	typedef btree_map_params<Key, Value, Compare, Alloc, TargetNodeSize> params_type;
	typedef btree<params_type> btree_type;
	typedef btree_map_container<btree_type> super_type;

public:
	typedef typename btree_type::key_compare key_compare;
	typedef typename btree_type::allocator_type allocator_type;
  typedef typename btree_type::iterator iterator;

public:
	// Default constructor.
	bmap(const key_compare& comp = key_compare(),
		const allocator_type& alloc = allocator_type())
		: super_type(comp, alloc) {
	}

	// Copy constructor.
	bmap(const self_type& x)
		: super_type(x) {
	}

	// Range constructor.
	template <class InputIterator>
	bmap(InputIterator b, InputIterator e,
		const key_compare& comp = key_compare(),
		const allocator_type& alloc = allocator_type())
		: super_type(b, e, comp, alloc) {
	}

  // shortcut to insert without having to create pair.  - BENTLEY Extension
  std::pair<iterator,bool> Insert(Key k, Value v) {return this->insert(pair_type(k,v));}

  bool operator== (self_type const& __y) const {return this->size() == __y.size() && std::equal(this->begin(), this->end(), __y.begin());}
  bool operator<  (self_type const& __y) const {return lexicographical_compare(this->begin(), this->end(), __y.begin(), __y.end());}
  bool operator!= (self_type const& __y) const {return !(*this == __y);}
  bool operator>  (self_type const& __y) const {return (__y < *this);}
  bool operator>= (self_type const& __y) const {return !(*this < __y);}
  bool operator<= (self_type const& __y) const {return !(__y < *this);}
};

template <typename K, typename V, typename C, int N, typename A>
inline void swap(bmap<K, V, C, N, A>& x, bmap<K, V, C, N, A>& y) {
	x.swap(y);
}

// The multimap class is needed mainly for its constructors.
template <typename Key, typename Value,
	typename Compare = std::less<Key>,
	uint16_t EntriesPerNode = 32,
	typename Alloc = dqBase::DqAllocator<std::pair<const Key, Value>>>
class bmultimap : public btree_multi_container<
	btree<btree_map_params<Key, Value, Compare, Alloc, EntriesPerNode*sizeof(bpair<Key, Value>)> > > {

  typedef std::pair<Key,Value> pair_type;
  enum { TargetNodeSize = EntriesPerNode*sizeof(pair_type)};

	typedef bmultimap<Key, Value, Compare, EntriesPerNode, Alloc> self_type;
	typedef btree_map_params< Key, Value, Compare, Alloc, TargetNodeSize> params_type;
	typedef btree<params_type> btree_type;
	typedef btree_multi_container<btree_type> super_type;

 public:
	typedef typename btree_type::key_compare key_compare;
	typedef typename btree_type::allocator_type allocator_type;
	typedef typename btree_type::data_type data_type;
	typedef typename btree_type::mapped_type mapped_type;
  typedef typename btree_type::iterator iterator;

 public:
	// Default constructor.
	bmultimap(const key_compare& comp = key_compare(),
			 const allocator_type& alloc = allocator_type())
		: super_type(comp, alloc) {
	}

	// Copy constructor.
	bmultimap(const self_type& x)
		: super_type(x) {
	}

	// Range constructor.
	template <class InputIterator>
	bmultimap(InputIterator b, InputIterator e,
			 const key_compare& comp = key_compare(),
			 const allocator_type& alloc = allocator_type())
		: super_type(b, e, comp, alloc) {
	}

  // shortcut to insert without having to create pair.  - BENTLEY Extension
  iterator Insert(Key k, Value v) {return this->insert(pair_type(k,v));}

  bool operator== (self_type const& __y) const {return this->size() == __y.size() && std::equal(this->begin(), this->end(), __y.begin());}
  bool operator<  (self_type const& __y) const {return lexicographical_compare(this->begin(), this->end(), __y.begin(), __y.end());}
  bool operator!= (self_type const& __y) const {return !(*this == __y);}
  bool operator>  (self_type const& __y) const {return (__y < *this);}
  bool operator>= (self_type const& __y) const {return !(*this < __y);}
  bool operator<= (self_type const& __y) const {return !(__y < *this);}
};

template <typename K, typename V, typename C, int N, typename A>
inline void swap(bmultimap<K, V, C, N, A>& x, bmultimap<K, V, C, N, A>& y) {
	x.swap(y);
}

END_DQ_BASE_NAMESPACE

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

