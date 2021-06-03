/*-------------------------------------------------------------------------
Compiler Generator Coco/R,
Copyright (c) 1990, 2004 Hanspeter Moessenboeck, University of Linz
extended by M. Loeberbauer & A. Woess, Univ. of Linz
ported to C++ by Csaba Balazs, University of Szeged
with improvements by Pat Terry, Rhodes University

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

As an exception, it is allowed to write an extension of Coco/R that is
used as a plugin in non-free software.

If not otherwise stated, any source code generated by Coco/R (other than
Coco/R itself) does not fall under the GNU General Public License.
-------------------------------------------------------------------------*/

#if !defined(COCO_ARRAYLIST_H__)
#define COCO_ARRAYLIST_H__

#include <stddef.h>

namespace Coco {

template<typename T>
class TArrayList
{
	T** Data;
public:
	typedef int tsize_t;
	tsize_t Count;
	tsize_t Capacity;

	TArrayList() {
                Count = 0;
                Capacity = 10;
                Data = new T*[ Capacity ];
        }
	virtual ~TArrayList() {
                delete [] Data;
        }

	void Add(T *value) {
                if (Count < Capacity) {
                        Data[Count] = value;
                        Count++;
                } else {
                        Capacity *= 2;
                        T** newData = new T*[Capacity];
                        for (tsize_t i=0; i<Count; i++) {
                                newData[i] = Data[i];		// copy
                        }
                        newData[Count] = value;
                        Count++;
                        delete [] Data;
                        Data = newData;
                }
        }

        //return the previous value
	void *Set(tsize_t index, T *value) {
                if (0<=index && index<Count) {
                        T *rv = Data[index];
                        Data[index] = value;
                        return rv;
                }
                return NULL;
        }

	void Remove(T *value) {
                for (tsize_t i=0; i<Count; i++) {
                        if (Data[i] == value) {
                                for (tsize_t j=i+1; j<Count; j++)
                                        Data[j-1] = Data[j];
                                Count--;
                                break;
                        }
                }
        }

	T *Pop() {
                if(Count == 0) return NULL;
                return Data[--Count];
        }

	T *Top() {
                if(Count == 0) return NULL;
                return Data[Count-1];
        }

	void Clear() {
                Count = 0;
        }

	T* operator[](tsize_t index) {
                if (0<=index && index<Count)
                        return Data[index];
                return NULL;
        }
};

typedef TArrayList<void> ArrayList;

}; // namespace

#endif // !defined(COCO_ARRAYLIST_H__)
