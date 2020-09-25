//
// Created by michele on 27/07/2020.
//

#ifndef CLIENT_CIRCULAR_VECTOR_H
#define CLIENT_CIRCULAR_VECTOR_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <atomic>

/**
 * generic and thread safe circular vector with functions to push, try to push, return (a reference) and pop an element to/from the vector
 *
 * @tparam T type of the element
 *
 * @author Michele Crepaldi s269551
 */
template <typename T>
class Circular_vector {
    std::mutex m;
    std::condition_variable cvPush, cvPop;
    std::vector<T> v;
    int start, end, size;

public:
    /**
     * simple constructor
     *
     * @param size of the circular vector (how many elements can be putted in the vector)
     *
     * @author Michele Crepaldi s269551
     */
    explicit Circular_vector(int size): start(0), end(0), size(size+1), v(size+1){
    }

    /**
     * function to push an object in the circular vector; if the circular vector is full it blocks the thread in passive waiting for it to be not full again
     *
     * @param t object to push in the circular vector
     *
     * @author Michele Crepaldi s269551
     */
    void push(T t){
        std::unique_lock l(m);
        cvPush.wait(l, [this](){return (end+1)%size != start;});
        v[end] = t;
        end = (end+1)%size;
        cvPop.notify_all();
    }

    /**
     * function which tries to push an object in the circular vector; if the circular vector is full then it immediately returns false,
     * otherwise it pushes the object and it returns true
     *
     * @param t object to push in the circular vector
     * @return whether the element was successfully pushed in the circular vector or not
     *
     * @author Michele Crepaldi s269551
     */
    bool tryPush(T t){
        std::unique_lock l(m);
        if(!cvPush.wait_for(l, std::chrono::milliseconds(0), [this](){return (end+1)%size != start;})) //wait for 0 --> test if the vector is not full
            return false; //if it is full return false (no object can be pushed now)

        //otherwise push object and return true
        v[end] = t;
        end = (end+1)%size;
        cvPop.notify_all();
        return true;
    }

    /**
     * function which returns a reference to the element in the head of the circular vector (it doesn't pop it)
     *
     * @return a reference to the first object inserted in the circular vector (the head of the circular vector)
     *
     * @author Michele Crepaldi s269551
     */
    T& front(){
        std::unique_lock l(m);
        cvPop.wait(l, [this](){return start != end;});
        return v[start];
    }

    /**
     * function which returns a reference to the element in the head of the circular vector (it doesn't pop it);
     * it also waits for timeout time before returning in any case
     * <p>
     * moreover the waiting can be stopped by the stop atomic boolean given in input, in such case it also returns nullopt
     * but for a different reason -> it will be the caller job to recognise it (looking at the stop atomic boolean)
     *
     * @param stop atomic boolean for witch to stop
     * @param timeout time to wait
     *
     * @return a reference to the first object inserted in the circular vector (the head of the circular vector) or a nullopt value (if timeout)
     *
     * @author Michele Crepaldi s269551
     */
    std::optional<T> frontWaitFor(int timeout, std::atomic<bool> &stop){
        std::unique_lock l(m);
        if (!cvPop.wait_for(l, std::chrono::seconds(timeout), [this, &stop](){return start != end || stop.load();})) //if false it means that it timed out!
            return std::nullopt;
        return std::optional<T>{v[start]};
    }

    /**
     * function used to know if there are elements that can be popped from the circular vector
     * or if we were told to stop.
     * <p>
     * It blocks the thread until something can be popped or if stop is true
     *
     * @param stop
     * @return true if there is an element, false if stop is true
     *
     * @author Michele Crepaldi s269551
*/
    bool waitForCondition(std::atomic<bool> &stop){
        std::unique_lock l(m);
        cvPop.wait(l, [this, &stop](){return start != end || stop.load();}); //stop will be always false, it will be true only when we want to close the program
        return !stop.load();
    }

    /**
     * function which pops the element in the head of the circular vector (it doesn't return it)
     *
     * @author Michele Crepaldi s269551
     */
    void pop(){
        std::unique_lock l(m);
        cvPop.wait(l, [this](){return start != end;});
        start = (start+1)%size;
        cvPush.notify_all();
    }
};


#endif //CLIENT_CIRCULAR_VECTOR_H
