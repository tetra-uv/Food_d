#ifndef INTQUEUE_H
#define INTQUEUE_H

// First-in first-out queue of ints in a circular array. Used for the arrival-order
// baseline.
class intqueue
{
public:
    // Creates an empty queue with room for start values.
    intqueue(int start = 16);

    // Makes a deep copy so both queues own separate arrays.
    intqueue(const intqueue& other);

    // Replaces this queue with a deep copy of another one.
    intqueue& operator=(const intqueue& other);

    // Frees the array.
    ~intqueue();

    // Adds a value at the back and doubles the array when it is full.
    void enqueue(int value);

    // Removes and returns the front value. Throws StructureException when empty.
    int dequeue();

    // Checks whether the queue has no values.
    bool empty() const;

    // Returns how many values are stored.
    int size() const;

private:
    // Array that holds the values
    int* items_;
    // Index of the first value in the queue
    int front_;
    // How many values are stored
    int count_;
    // Size of the array
    int capacity_;

    // Doubles the capacity and copies the values in queue order.
    void grow();
};

#endif
