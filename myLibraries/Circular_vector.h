//
// Created by Michele Crepaldi s269551 on 27/07/2020
// Finished on 21/11/2020
// Last checked on 21/11/2020
//

#ifndef CIRCULAR_VECTOR_H
#define CIRCULAR_VECTOR_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <atomic>


/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * Circular_vector class
 */

/**
 * Circular_vector class. Generic circular vector.
 *
 * @tparam T type of the elements inside the vector
 *
 * @author Michele Crepaldi s269551
 */
template <typename T>
class Circular_vector {
public:
    Circular_vector(const Circular_vector &) = delete;    //copy constructor deleted
    Circular_vector& operator=(const Circular_vector &) = delete; //assignment deleted
    Circular_vector(Circular_vector &&) = delete; //move constructor deleted
    Circular_vector& operator=(Circular_vector &&) = delete;  //move assignment deleted
    ~Circular_vector() = default;

    /**
     * Circular_vector operator [] override.
     *
     * @param i index of the element to retrieve
     * @return reference to the element requested
     *
     * @author Michele Crepaldi s269551
     */
    T& operator[](int i){
        return _v[i];
    }

    /**
     * simple constructor
     *
     * @param size of the circular vector (how many elements can be put in the vector)
     *
     * @author Michele Crepaldi s269551
     */
    explicit Circular_vector(unsigned int size): _start(0), _end(0), _capacity(size + 1){
        _v.resize(_capacity + 1);
    }

    /**
     * method used to push an object in the circular vector.
     *
     * @param t object to push in the circular vector
     *
     * @author Michele Crepaldi s269551
     */
    void push(T t){
        _v[_end] = std::move(t);    //move the object into the circular vector
        _end = (_end + 1) % _capacity;  //update _end
    }

    /**
     * method used to return a reference to the element in the head of the circular vector (it doesn't pop it)
     *
     * @return a reference to the first object of the circular vector (the head of the circular vector)
     *
     * @author Michele Crepaldi s269551
     */
    T& front(){
        return _v[_start];  //return the reference to the first element (head of the queue)
    }

    /**
     * method used to pop the element in the head of the circular vector (it doesn't return it)
     *
     * @author Michele Crepaldi s269551
     */
    void pop(){
        //remove the element from the circular vector but do not return it (it will be destroyed in this way)

        T tmp = std::move(_v[_start]);  //current element

        _start = (_start + 1) % _capacity;  //update _start (effectively popping one element)
    }

    /**
     * method used to pop the element in the head of the circular vector and return it (with move)
     *
     * @return the first object (the head) in the circular vector (by movement)
     *
     * @author Michele Crepaldi s269551
     */
    T get(){
        T tmp = std::move(_v[_start]);  //current element
        _start = (_start + 1) % _capacity;  //update _start (effectively popping one element)

        return std::move(tmp);  //return element by movement
    }

    /**
     * method used to get the index of the head element in the circular vector
     *
     * @return index of the first element
     *
     * @author Michele Crepaldi s269551
     */
    [[nodiscard]] int start() const{
        return _start;
    }

    /**
     * method used to get the index of the tail element in the circular vector
     *
     * @return index of the last element
     *
     * @author Michele Crepaldi s269551
     */
    [[nodiscard]] int end() const {
        return _end;
    }

    /**
     * method used to know if the circular vector is empty or not
     *
     * @return true if the circular vector is empty, false otherwise
     *
     * @author Michele Crepaldi s269551
     */
    [[nodiscard]] bool empty() const{
        return _start == _end;
    }

    /**
     * method used to know if the circular vector is full or not
     *
     * @return true if the circular vector is full, false otherwise
     *
     * @author Michele Crepaldi s269551
     */
    [[nodiscard]] bool full() const{
        return (_end+1) % _capacity == _start;
    }

    /**
     * method used to know the current number of elements inside the circular vector
     *
     * @return number of elements stored in the circular vector
     *
     * @author Michele Crepaldi s269551
     */
    [[nodiscard]] int size() const{
        return (_capacity + _end - _start) % _capacity;
    }

private:
    std::vector<T> _v;   //circular vector
    int _start; //head of the circular vector
    int _end; //tail of the circular vector
    unsigned int _capacity; //max size of the circular vector
};

/*
 * +-------------------------------------------------------------------------------------------------------------------+
 * TS_Circular_vector class
 */

/**
 * TS_Circular_vector class. Thread safe version of the generic circular vector.
 *
 * @tparam T type of the elements inside the vector
 *
 * @author Michele Crepaldi s269551
 */
template <typename T>
class TS_Circular_vector {
public:
    TS_Circular_vector(const TS_Circular_vector &) = delete;    //copy constructor deleted
    TS_Circular_vector& operator=(const TS_Circular_vector &) = delete; //assignment deleted
    TS_Circular_vector(TS_Circular_vector &&) = delete; //move constructor deleted
    TS_Circular_vector& operator=(TS_Circular_vector &&) = delete;  //move assignment deleted
    ~TS_Circular_vector() = default;

    /**
     * simple constructor
     *
     * @param size of the circular vector (how many elements can be put in the vector)
     *
     * @author Michele Crepaldi s269551
     */
    explicit TS_Circular_vector(unsigned int size): _start(0), _end(0), _size(size + 1){
        _v.resize(size + 1);
    }

    /**
     * method used to push an object in the circular vector;
     *  if the circular vector is full it blocks the thread in passive waiting
     *
     * @param t object to push in the circular vector
     *
     * @author Michele Crepaldi s269551
     */
    void push(T t){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable

        //wait on _cvPush condition variable until there is an empty space in the circular vector
        _cvPush.wait(l, [this](){return (_end + 1) % _size != _start;});

        _v[_end] = std::move(t);    //move the object into the circular vector
        _end = (_end + 1) % _size;  //update _end
        _cvPop.notify_all();        //notify _cvPop
    }

    /**
     * method used to try to push an object in the circular vector;
     *  if the circular vector is full then it immediately returns false,
     *  otherwise it pushes the object and it returns true
     *
     * @param t object to push in the circular vector
     * @return whether the element was successfully pushed in the circular vector or not
     *
     * @author Michele Crepaldi s269551
     */
    bool tryPush(T t){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable

        //wait on _cvPush condition variable for 0 milliseconds (so test if the circular vector is not full)
        if(!_cvPush.wait_for(l, std::chrono::milliseconds(0), [this](){return (_end + 1) % _size != _start;}))
            return false; //if it is full return false (no object can be pushed now)

        //otherwise push object and return true

        _v[_end] = std::move(t);    //move the object into the circular vector
        _end = (_end + 1) % _size;  //update _end
        _cvPop.notify_all();        //notify _cvPop
        return true;
    }

    /**
     * method used to return a reference to the element in the head of the circular vector (it doesn't pop it)
     *
     * @return a reference to the first object of the circular vector (the head of the circular vector)
     *
     * @author Michele Crepaldi s269551
     */
    T& front(){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable

        //wait on _cvPop condition variable until there is at least one element in the circular vector
        _cvPop.wait(l, [this](){return _start != _end;});
        return _v[_start];
    }

    /**
     * method used to know if there are elements that can be popped from the circular vector
     *  or if we were told to stop
     *  <p>
     *  It blocks the thread until something can be popped or stop atomic boolean becomes true
     *
     * @param stop atomic boolean used to stop the wait
     * @return true if there is an element, false if stop is true
     *
     * @author Michele Crepaldi s269551
    */
    bool waitForCondition(std::atomic<bool> &stop){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable

        //wait on _cvPop condition variable until there is at least one element in the circular vector OR
        //stop atomic boolean is true
        _cvPop.wait(l, [this, &stop](){return _start != _end || stop.load();});

        //stop will be always false, it will be true only when we want to close the program
        return !stop.load();    //so return true if an element can be popped, false if stop is true
    }


    /**
     * method used to pop the element in the head of the circular vector (it doesn't return it)
     *
     * @author Michele Crepaldi s269551
     */
    void pop(){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable

        //wait on _cvPop condition variable until there is at least one element in the circular vector
        _cvPop.wait(l, [this](){return _start != _end;});

        //remove the element from the circular vector but do not return it (it will be destroyed in this way)

        T tmp = std::move(_v[_start]);  //current element

        _start = (_start + 1) % _size;  //update _start (effectively popping one element)

        _cvPush.notify_all();   //notify _cvPush
    }

    /**
     * method used to pop the element in the head of the circular vector and return it (with move)
     *
     * @return the first object (the head) in the circular vector (by movement)
     *
     * @author Michele Crepaldi s269551
     */
    T get(){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable

        //wait on _cvPop condition variable until there is at least one element in the circular vector
        _cvPop.wait(l, [this](){return _start != _end;});

        T tmp = std::move(_v[_start]);  //current element
        _start = (_start + 1) % _size;  //update _start (effectively popping one element)

        _cvPush.notify_all();   //notify _cvPush
        return std::move(tmp);  //return element by movement
    }

    /**
     * method used to try to pop the element in the head of the circular vector and return it (with move);
     *  It will return an optional containing the moved element or nothing in case the method stopped
     *  because the stop atomic boolean became true
     *
     * @return an optional containing the first object (the head) in the circular vector (by movement) or nothing
     *
     * @author Michele Crepaldi s269551
     */
    std::optional<T> tryGetUntil(std::atomic<bool> &stop){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable

        //wait on _cvPop condition variable until there is at least one element in the circular vector
        _cvPop.wait(l, [this, &stop](){return _start != _end || stop.load();});

        //if I woke up thanks to the stop atomic boolean
        if(stop.load()) {
            notifyAll();    //notify all threads
            return std::nullopt;    //return nullopt
        }

        T tmp = std::move(_v[_start]);  //current element
        _start = (_start + 1) % _size;  //update _start (effectively popping one element)

        _cvPush.notify_all();   //notify _cvPush
        return std::move(tmp);  //return element by movement
    }

    /**
     * method used to know if the queue has at least one element or not
     *
     * @return true if there is at least one element in the vector, false otherwise
     *
     * @author Michele Crepaldi s269551
     */
    bool canGet(){
        std::unique_lock l(_m); //unique lock to ensure thread safeness and to be used with the condition variable
        return _start != _end;  //true if there is at least one element, false otherwise
    }

    /**
     * method used to notify all thread in passive waiting on the condition variables
     *
     * @author Michele Crepaldi s269551
     */
    void notifyAll(){
        _cvPop.notify_all();
        _cvPush.notify_all();
    }

private:
    std::mutex _m;   //mutex to be used with the conditional variables
    std::condition_variable _cvPush; //condition variable to be used to push elements into the circular vector
    std::condition_variable _cvPop;  //condition variable to be used to pop elements from the circular vector
    std::vector<T> _v;   //circular vector
    int _start; //head of the circular vector
    int _end; //tail of the circular vector
    unsigned int _size; //size of the circular vector
};


#endif //CIRCULAR_VECTOR_H
