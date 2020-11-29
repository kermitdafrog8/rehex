/* Reverse Engineer's Hex Editor
 * Copyright (C) 2020 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef REHEX_BYTERANGESET_HPP
#define REHEX_BYTERANGESET_HPP

#include <assert.h>
#include <iterator>
#include <sys/types.h>
#include <vector>

namespace REHex
{
	/**
	 * @brief Wrapper around std::set for storing ranges in a file.
	 *
	 * This class is a wrapper around std::set that can be used for storing ranges. Any ranges
	 * which are adjacent or overlapping will be merged to reduce memory consumption, so only
	 * each unique contiguous range added will take space in memory.
	*/
	class ByteRangeSet
	{
		public:
			/**
			 * @brief A range stored within a ByteRangeSet.
			*/
			struct Range
			{
				off_t offset;
				off_t length;
				
				Range(off_t offset, off_t length):
					offset(offset), length(length) {}
				
				bool operator<(const Range &rhs) const
				{
					if(offset != rhs.offset)
					{
						return offset < rhs.offset;
					}
					else{
						return length < rhs.length;
					}
				}
				
				bool operator==(const Range &rhs) const
				{
					return offset == rhs.offset && length == rhs.length;
				}
			};
			
		private:
			std::vector<Range> ranges;
			
		public:
			/**
			 * @brief Construct an empty set.
			*/
			ByteRangeSet() {}
			
			ByteRangeSet(const ByteRangeSet &src):
				ranges(src.ranges) {}
			
			/**
			 * @brief Construct a set from a sequence of ranges.
			 *
			 * NOTE: The ranges MUST be in order and MUST NOT be adjacent.
			*/
			template<typename T> ByteRangeSet(const T begin, const T end):
				ranges(begin, end) {}
			
			/**
			 * @brief Set a range of bytes in the set.
			 *
			 * This method adds a range of bytes to the set. Any existing ranges
			 * adjacent to or within the new range will be merged into the new range
			 * and removed from the set.
			*/
			void set_range(off_t offset, off_t length);
			
			/**
			 * @brief Set multiple ranges of bytes in the set.
			 *
			 * This method takes a pair of Range iterators (or pointers) and adds all
			 * of the ranges to the set. It is more efficient than calling set_range()
			 * multiple times.
			 *
			 * NOTE: The ranges MUST be in order and MUST NOT be adjacent.
			 *
			 * If size_hint is provided and the internal vector is near its limit, the
			 * vector's capacity will be expanded to size_hint elements instead of only
			 * growing enough to accomodate this operation.
			*/
			template<typename T> void set_ranges(const T begin, const T end, size_t size_hint = 0);
			
			/**
			 * @brief Clear a range of bytes in the set.
			 *
			 * This method clears a range of bytes in the set. Ranges within the set
			 * will be split if necessary to preserve bytes outside of the range to be
			 * cleared.
			*/
			void clear_range(off_t offset, off_t length);
			
			/**
			 * @brief Clear multiple ranges of bytes in the set.
			 *
			 * This method takes a pair of Range iterators (or pointers) and removes
			 * all of the ranges from the set. It is more efficient than calling
			 * clear_range() multiple times.
			 *
			 * NOTE: The ranges MUST be in order, MUST NOT be adjacent and MUST NOT be
			 * within the set itself.
			*/
			template<typename T> void clear_ranges(const T begin, const T end);
			
			/**
			 * @brief Clear all ranges in the set.
			*/
			void clear_all();
			
			/**
			 * @brief Check if a byte is set in the set.
			*/
			bool isset(off_t offset) const;
			
			/**
			 * @brief Get a reference to the internal std::set.
			*/
			const std::vector<Range> &get_ranges() const;
			
			/**
			 * @brief Returns a const_iterator to the first Range in the set.
			*/
			std::vector<Range>::const_iterator begin() const;
			
			/**
			 * @brief Returns a const_iterator to the end of the set.
			*/
			std::vector<Range>::const_iterator end() const;
			
			/**
			 * @brief Access the n-th range in the set.
			*/
			const Range &operator[](size_t idx) const;
			
			/**
			 * @brief Returns the number of ranges in the set.
			*/
			size_t size() const;
			
			/**
			 * @brief Returns true if the set is empty.
			*/
			bool empty() const;
			
			/**
			 * @brief Adjust for data being inserted into file.
			 *
			 * Ranges after the insertion will be moved along by the size of the
			 * insertion. Ranges spanning the insertion will be split.
			*/
			void data_inserted(off_t offset, off_t length);
			
			/**
			 * @brief Minimum number of ranges to make data_inserted() use threads.
			*/
			static const size_t DATA_INSERTED_THREAD_MIN = 100000;
			
			/**
			 * @brief Adjust for data being erased from file.
			 *
			 * Ranges after the section erased will be moved back by the size of the
			 * insertion. Ranges wholly within the erased section will be lost. Ranges
			 * on either side of the erase will be truncated and merged as necessary.
			*/
			void data_erased(off_t offset, off_t length);
			
			/**
			 * @brief Find the intersection of two sets.
			 *
			 * Returns a ByteRangeSet containing only the ranges of bytes which are set
			 * in BOTH sets.
			*/
			static ByteRangeSet intersection(const ByteRangeSet &a, const ByteRangeSet &b);
	};
}

template<typename T> void REHex::ByteRangeSet::set_ranges(const T begin, const T end, size_t size_hint)
{
	size_t min_size_hint = ranges.size() + std::distance(begin, end);
	if(ranges.capacity() < min_size_hint)
	{
		ranges.reserve(std::max(min_size_hint, size_hint));
	}
	
	auto next = ranges.begin();
	
	/* Existing elements which intersect the ones we are inserting are erased and the new ones
	 * expand to encompass the whole range.
	 *
	 * Adjacent erase and insert operations are merged for performance. The group_erase_begin
	 * and group_erase_end iterators encompass the full range of adjacent elements to be erased
	 * from sequential inserts and group_ranges contains all adjacent elements to be inserted.
	*/
	std::vector<Range>::iterator group_erase_begin;
	std::vector<Range>::iterator group_erase_end;
	std::vector<Range> group_ranges;
	
	for(auto r = begin; r != end;)
	{
		assert(r == begin || (std::prev(r)->offset + std::prev(r)->length) < r->offset);
		
		off_t offset = r->offset;
		off_t length = r->length;
		
		/* Find the range of elements that intersects the one we are inserting. They will be erased
		 * and the one we are creating will grow on either end as necessary to encompass them.
		*/
		
		next = std::lower_bound(next, ranges.end(), Range((offset + length), 0));
		
		std::vector<Range>::iterator erase_begin = next;
		std::vector<Range>::iterator erase_end   = next;
		
		while(erase_begin != ranges.begin())
		{
			auto eb_prev = std::prev(erase_begin);
			
			if((eb_prev->offset + eb_prev->length) >= offset)
			{
				off_t merged_begin = std::min(eb_prev->offset, offset);
				off_t merged_end   = std::max((eb_prev->offset + eb_prev->length), (offset + length));
				
				offset = merged_begin;
				length = merged_end - merged_begin;
				
				erase_begin = eb_prev;
			}
			else{
				break;
			}
		}
		
		if(erase_end != ranges.end() && erase_end->offset == (offset + length))
		{
			length += erase_end->length;
			++erase_end;
		}
		
		assert(length > 0);
		
		if(!group_ranges.empty() && erase_begin != group_erase_end)
		{
			/* We have elements pending to be inserted earlier in the vector, flush
			 * them out so we can start a new group.
			*/
			
			next = ranges.erase(group_erase_begin, group_erase_end);
			
			/* Workaround for older GCC/libstd++ which have the wrong return type
			 * (void) on multi-element std::vector::insert(), even under C++11 mode.
			 *
			 * Not 100% sure which version actually fixed it.
			*/
			
			#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9))
			for(auto i = group_ranges.begin(); i != group_ranges.end(); ++i)
			{
				next = ranges.insert(next, *i);
				++next;
			}
			#else
			next = ranges.insert(next, group_ranges.begin(), group_ranges.end());
			std::advance(next, group_ranges.size());
			#endif
			
			group_ranges.clear();
			
			/* The erase and insert operations have invalidated the erase_begin and
			 * erase_end iterators, so start processing this Range again.
			*/
			continue;
		}
		
		if(group_ranges.empty())
		{
			/* We are starting a new range of insertions, initialize group_erase_begin
			 * to the erase_begin of this range.
			*/
			group_erase_begin = erase_begin;
		}
		
		/* Advance group_erase_end to the end of this range's erase block. */
		group_erase_end = erase_end;
		
		group_ranges.push_back(Range(offset, length));
		
		++r;
	}
	
	if(!group_ranges.empty())
	{
		/* Flush pending erase/insert operations. */
		
		next = ranges.erase(group_erase_begin, group_erase_end);
		ranges.insert(next, group_ranges.begin(), group_ranges.end());
	}
}

template<typename T> void REHex::ByteRangeSet::clear_ranges(const T begin, const T end)
{
	auto next = ranges.begin();
	
	/* Existing elements which intersect the ones we are clearing are erased and any adjacent
	 * bytes which become cleared as a side effect are re-inserted as new ranges.
	 *
	 * Adjacent erase and insert operations are merged for performance. The group_erase_begin
	 * and group_erase_end iterators encompass the full range of adjacent elements to be erased
	 * from sequential inserts and group_replacements contains all adjacent ranges to be
	 * re-inserted at the same position.
	*/
	std::vector<Range>::iterator group_erase_begin = ranges.end();
	std::vector<Range>::iterator group_erase_end   = ranges.end();
	std::vector<Range> group_replacements;
	
	for(auto r = begin; r != end;)
	{
		assert(r == begin || (std::prev(r)->offset + std::prev(r)->length) < r->offset);
		
		off_t offset = r->offset;
		off_t length = r->length;
		
		/* Find the range of elements overlapping the range to be cleared. */
		
		next = std::lower_bound(next, ranges.end(), Range((offset + length), 0));
		
		std::vector<Range>::iterator erase_begin = next;
		std::vector<Range>::iterator erase_end   = next;
		
		while(erase_begin != ranges.begin())
		{
			auto eb_prev = std::prev(erase_begin);
			
			if((eb_prev->offset + eb_prev->length) >= offset)
			{
				erase_begin = eb_prev;
			}
			else{
				break;
			}
		}
		
		if(group_erase_begin != group_erase_end)
		{
			if(erase_begin != group_erase_end)
			{
				/* We have elements pending to be erased/replaced earlier in the
				 * vector, flush them out so we can start a new group.
				*/
				
				next = ranges.erase(group_erase_begin, group_erase_end);
				
				/* Workaround for older GCC/libstd++ which have the wrong return type
				 * (void) on multi-element std::vector::insert(), even under C++11 mode.
				 *
				 * Not 100% sure which version actually fixed it.
				*/
				
				#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9))
				for(auto i = group_replacements.begin(); i != group_replacements.end(); ++i)
				{
					next = ranges.insert(next, *i);
					++next;
				}
				#else
				next = ranges.insert(next, group_replacements.begin(), group_replacements.end());
				std::advance(next, group_replacements.size());
				#endif
				
				group_erase_begin = ranges.end();
				group_erase_end   = ranges.end();
				group_replacements.clear();
				
				/* The erase and insert operations have invalidated the erase_begin
				 * and erase_end iterators, so start processing this Range again.
				*/
				continue;
			}
		}
		else{
			group_erase_begin = erase_begin;
		}
		
		/* Advance group_erase_end to the end of this range's erase block. */
		group_erase_end = erase_end;
		
		/* If the elements to be erased to not fall fully within the given range, then we shall
		 * populate group_replacements with up to two elements to re-instate the lost ranges.
		*/
		
		if(erase_begin != erase_end)
		{
			if(erase_begin->offset < offset)
			{
				/* Clear range is within erase range, so create a new Range from
				 * the start of the range to be erased up to the start of the range
				 * to be cleared.
				*/
				
				group_replacements.push_back(Range(erase_begin->offset, (offset - erase_begin->offset)));
			}
			
			auto erase_last = std::prev(erase_end);
			
			if((erase_last->offset + erase_last->length) > (offset + length))
			{
				/* Clear range falls short of the end of the range to be erased, so
				 * create a range from the end of the clear range to the end of the
				 * erase range.
				*/
				
				off_t from = offset + length;
				off_t to   = erase_last->offset + erase_last->length;
				
				assert(to > from);
				
				group_replacements.push_back(Range(from, (to - from)));
			}
		}
		
		++r;
	}
	
	
	/* Flush pending erase/insert operations. */
	
	next = ranges.erase(group_erase_begin, group_erase_end);
	ranges.insert(next, group_replacements.begin(), group_replacements.end());
}

#endif /* !REHEX_BYTERANGESET_HPP */
