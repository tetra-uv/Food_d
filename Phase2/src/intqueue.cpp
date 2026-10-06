#include "intqueue.h"
#include "Exceptions.h"

using namespace std;

// Creates an empty queue with room for start values.
intqueue::intqueue(int start)
{
    if (start < 1)
    {
        throw StructureException("Queue capacity must be at least 1");
    }

    items_ = new int[start];
    front_ = 0;
    count_ = 0;
    capacity_ = start;
}

// Copies the values in queue order, so the copy starts at index 0.
intqueue::intqueue(const intqueue& other)
{
    items_ = new int[other.capacity_];
    front_ = 0;
    count_ = other.count_;
    capacity_ = other.capacity_;

    for (int pos = 0; pos < count_; pos++)
    {
        items_[pos] = other.items_[(other.front_ + pos) % other.capacity_];
    }
}

// Copies the other queue after the new array is ready, then frees the old one.
intqueue& intqueue::operator=(const intqueue& other)
{
    if (this != &other)
    {
        int* fresh = new int[other.capacity_];

        for (int pos = 0; pos < other.count_; pos++)
        {
            fresh[pos] = other.items_[(other.front_ + pos) % other.capacity_];
        }

        delete[] items_;
        items_ = fresh;
        front_ = 0;
        count_ = other.count_;
        capacity_ = other.capacity_;
    }

    return *this;
}

// Frees the array.
intqueue::~intqueue()
{
    delete[] items_;
    items_ = NULL;
}

// Stores the value after the last one, wrapping to index 0 at the end of the array.
void intqueue::enqueue(int value)
{
    if (count_ == capacity_)
    {
        grow();
    }

    int back = (front_ + count_) % capacity_;
    items_[back] = value;
    count_++;
}

// Returns the front value and moves the front forward, wrapping at the end of the array.
int intqueue::dequeue()
{
    if (count_ == 0)
    {
        throw StructureException("dequeue on an empty intqueue");
    }

    int value = items_[front_];
    front_ = (front_ + 1) % capacity_;
    count_--;
    return value;
}

// Checks whether the queue has no values.
bool intqueue::empty() const
{
    return count_ == 0;
}

// Returns how many values are stored.
int intqueue::size() const
{
    return count_;
}

// Copies the values in queue order into a bigger array so the front is index 0 again.
void intqueue::grow()
{
    int* bigger = new int[capacity_ * 2];

    for (int pos = 0; pos < count_; pos++)
    {
        bigger[pos] = items_[(front_ + pos) % capacity_];
    }

    delete[] items_;
    items_ = bigger;
    front_ = 0;
    capacity_ = capacity_ * 2;
}
