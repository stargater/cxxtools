/*
 * Copyright (C) 2010 Tommi Maekitalo
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef CXXTOOLS_QUEUE_H
#define CXXTOOLS_QUEUE_H

#include <queue>
#include <cxxtools/mutex.h>
#include <cxxtools/condition.h>
#include <cxxtools/timespan.h>
#include <cxxtools/scopedincrement.h>

namespace cxxtools
{
    /** @brief This class implements a thread safe queue.

        A queue is a container where the elements put into the queue are
        fetched in the same order (first-in-first-out, fifo).
        The class has a optional maximum size. If the size is set to 0 the
        queue has no limit. Otherwise putting a element to the queue may
        block until another thread fetches a element or increases the limit.
     */
    template <typename T>
    class Queue
    {
        public:
            typedef T value_type;
            typedef typename std::deque<T>::size_type size_type;
            typedef typename std::deque<T>::const_reference const_reference;

        private:
            mutable Mutex _mutex;
            mutable Condition _notEmpty;
            mutable Condition _notFull;
            std::deque<value_type> _queue;
            size_type _maxSize;
            size_type _numWaiting;

        public:
            /// @brief Default Constructor.
            Queue()
                : _maxSize(0),
                  _numWaiting(0)
            { }

            /** @brief Returns the next element.

                This method returns the next element. If the queue is empty,
                the thread will be locked until a element is available.
             */
            value_type get();

            /** @brief Returns the next element if the queue is not empty.

                This method returns the next element. If the queue is empty,
                the thread will wait up to timeout milliseconds until a element
                is available.

                If the queue was empty after the timeout, a pair of a default
                constructed value_type and the value false are returned.
                Otherwise the next element is removed and returned together
                with a true value.

                Spurious wakeups are ignored so that when that happens, the
                method might return before the timeout is over even when the
                queue is empty.
             */
            std::pair<value_type, bool> get(const Milliseconds& timeout);

            /** @brief Returns the next element if the queue is not empty.

                If the queue is empty, a default constructed value_type is returned.
                The returned flag is set to false, if the queue was empty.
             */
            std::pair<value_type, bool> tryGet();

            /** @brief Adds a element to the queue.

                This method adds a element to the queue. If the queue has
                reached his maximum size, the method blocks until there is
                space available.
             */
            void put(const_reference element, bool force = false);

            /** @brief Removes one specific element.

                This method removes one specific element from the queue.
                It returns true, when the element was found and removed.
             */
            bool remove(const_reference element);

            /// @brief Returns true, if the queue is empty.
            bool empty() const
            {
                MutexLock lock(_mutex);
                return _queue.empty();
            }

            /// @brief Waits until timeout or queue empty.
            /// Waits until timeout or queue empty. Returns true
            /// when queue is not empty.
            bool waitEmpty(Milliseconds timeout = -1) const
            {
                MutexLock lock(_mutex);
                if (timeout >= Milliseconds(0))
                {
                    Timespan until = Timespan::gettimeofday() + timeout;
                    Timespan remaining;
                    while (!_queue.empty()
                      && (remaining = until - Timespan::gettimeofday()) > Timespan(0))
                        _notEmpty.wait(_mutex, remaining);
                }
                else
                {
                    while (!_queue.empty())
                        _notEmpty.wait(lock);
                }

                return !_queue.empty();
            }

            /// @brief Returns the number of elements currently in queue.
            size_type size() const
            {
                MutexLock lock(_mutex);
                return _queue.size();
            }

            /** @brief sets the maximum size of the queue.

                Setting the maximum size of the queue may wake up another
                thread, if it is waiting for space to get available and the
                limit is increased.
             */
            void maxSize(size_type m);

            /// @brief returns the maximum size of the queue.
            size_type maxSize() const
            {
                MutexLock lock(_mutex);
                return _maxSize;
            }

            /// @brief returns the number of threads blocked in the get method.
            size_type numWaiting() const
            {
                MutexLock lock(_mutex);
                return _numWaiting;
            }
    };

    template <typename T>
    typename Queue<T>::value_type Queue<T>::get()
    {
        MutexLock lock(_mutex);

        ScopedIncrement<size_type> inc(_numWaiting);
        while (_queue.empty())
            _notEmpty.wait(lock);

        value_type element = _queue.front();
        _queue.pop_front();

        if (!_queue.empty())
            _notEmpty.signal();

        _notFull.signal();

        return element;
    }

    template <typename T>
    std::pair<typename Queue<T>::value_type, bool> Queue<T>::get(const Milliseconds& timeout)
    {
        typedef typename Queue<T>::value_type value_type;
        typedef typename std::pair<value_type, bool> return_type;

        MutexLock lock(_mutex);

        ScopedIncrement<size_type> inc(_numWaiting);
        if (_queue.empty())
        {
            if (_notEmpty.wait(lock, timeout) == false)
            {
                return return_type(value_type(), false);
            }
        }

        value_type element = _queue.front();
        _queue.pop_front();

        if (!_queue.empty())
            _notEmpty.signal();

        _notFull.signal();

        return return_type(element, true);
    }

    template <typename T>
    std::pair<typename Queue<T>::value_type, bool> Queue<T>::tryGet()
    {
        typedef typename Queue<T>::value_type value_type;
        typedef typename std::pair<value_type, bool> return_type;

        MutexLock lock(_mutex);

        if (_queue.empty())
            return return_type(value_type(), false);

        value_type element = _queue.front();
        _queue.pop_front();

        if (!_queue.empty())
            _notEmpty.signal();

        _notFull.signal();

        return return_type(element, true);
    }

    template <typename T>
    void Queue<T>::put(typename Queue<T>::const_reference element, bool force)
    {
        MutexLock lock(_mutex);

        if (!force)
            while (_maxSize > 0 && _queue.size() >= _maxSize)
                _notFull.wait(lock);

        _queue.push_back(element);
        _notEmpty.signal();

        if (_maxSize > 0 && _queue.size() < _maxSize)
            _notFull.signal();
    }

    template <typename T>
    bool Queue<T>::remove(const_reference element)
    {
        MutexLock lock(_mutex);
        for (typename std::deque<T>::iterator it = _queue.begin(); it != _queue.end(); ++it)
            if (*it == element)
                return true;
        return false;
    }

    template <typename T>
    void Queue<T>::maxSize(size_type m)
    {
        _maxSize = m;
        MutexLock lock(_mutex);
        if (_queue.size() < _maxSize)
            _notFull.signal();
    }
}

#endif // CXXTOOLS_QUEUE_H

