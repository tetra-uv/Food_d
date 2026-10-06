#ifndef SORTING_H
#define SORTING_H

using namespace std;

// Compares two values with operator<. Used when no comparison function is given.
template <class T>
bool lessbyoperator(const T& first, const T& second)
{
    return first < second;
}

// Merges the sorted halves items[low..middle] and items[middle+1..high] through temp.
// The left value wins a tie, which keeps equal values in their original order.
template <class T>
void mergejoin(T* items, T* temp, int low, int middle, int high,
               bool (*before)(const T&, const T&))
{
    int left = low;
    int right = middle + 1;
    int pos = low;

    while (left <= middle && right <= high)
    {
        if (before(items[right], items[left]))
        {
            temp[pos] = items[right];
            right++;
        }
        else
        {
            temp[pos] = items[left];
            left++;
        }

        pos++;
    }

    while (left <= middle)
    {
        temp[pos] = items[left];
        left++;
        pos++;
    }

    while (right <= high)
    {
        temp[pos] = items[right];
        right++;
        pos++;
    }

    for (pos = low; pos <= high; pos++)
    {
        items[pos] = temp[pos];
    }
}

// Splits items[low..high] in two halves, sorts each half recursively and joins them.
template <class T>
void mergepart(T* items, T* temp, int low, int high, bool (*before)(const T&, const T&))
{
    if (low >= high)
    {
        return;
    }

    int middle = (low + high) / 2;
    mergepart(items, temp, low, middle, before);
    mergepart(items, temp, middle + 1, high, before);
    mergejoin(items, temp, low, middle, high, before);
}

// Sorts count values in place with merge sort.
// before(a, b) must be true when a goes first.
template <class T>
void mergesort(T* items, int count, bool (*before)(const T&, const T&))
{
    if (count < 2)
    {
        return;
    }

    T* temp = new T[count];
    mergepart(items, temp, 0, count - 1, before);
    delete[] temp;
}

// Sorts count values in place with merge sort, using operator< of the type.
template <class T>
void mergesort(T* items, int count)
{
    mergesort(items, count, lessbyoperator<T>);
}

#endif