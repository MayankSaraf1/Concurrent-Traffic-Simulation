#include <iostream>
#include <random>
#include "TrafficLight.h"

/* Implementation of class "MessageQueue" */

template <typename T>
T MessageQueue<T>::receive() //pop
{
    // FP.5a : The method receive should use std::unique_lock<std::mutex> and _condition.wait() 
    // to wait for and receive new messages and pull them from the queue using move semantics. 
    // The received object should then be returned by the receive function. 

    // perform queue modification under the lock
    std::unique_lock<std::mutex> uLock(_mutex);
    _condition.wait(uLock, [this] { return !_queue.empty(); }); // pass unique lock to condition variable

    // remove last element from queue
    T msg = std::move(_queue.back());
    _queue.pop_back();

    return msg;
}

template <typename T>
void MessageQueue<T>::send(T &&msg) // push
{
    // FP.4a : The method send should use the mechanisms std::lock_guard<std::mutex> 
    // as well as _condition.notify_one() to add a new message to the queue and afterwards send a notification.

    // perform vector modification under the lock
    std::lock_guard<std::mutex> uLock(_mutex);

    _queue.push_back(std::move(msg));
    _condition.notify_one(); // notify client after pushing new message into vector
}


/* Implementation of class "TrafficLight" */


TrafficLight::TrafficLight()
{
    _currentPhase = TrafficLightPhase::red;
}

 
void TrafficLight::waitForGreen()
{
    // FP.5b : add the implementation of the method waitForGreen, in which an infinite while-loop 
    // runs and repeatedly calls the receive function on the message queue. 
    // Once it receives TrafficLightPhase::green, the method returns.
    while(true)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        TrafficLightPhase currentPhase = _queueMgr.receive();
        if (currentPhase == TrafficLightPhase::green)
        {
            //std::cout << "Phase: " << (int)currentPhase << " ID: " << std::this_thread::get_id() << "\n";
            return;
        }
    }
}

TrafficLightPhase TrafficLight::getCurrentPhase()
{
    return _currentPhase;
}

void TrafficLight::simulate()
{
    // FP.2b : Finally, the private method „cycleThroughPhases“ should be started in a thread when the public method „simulate“ is called. To do this, use the thread queue in the base class. 

    // launch cycleThroughPhases function in a thread
    threads.emplace_back(std::thread(&TrafficLight::cycleThroughPhases, this));
}


// virtual function which is executed in a thread
void TrafficLight::cycleThroughPhases()
{
    // std::cout <<" Traffic Light ID: " << std::this_thread::get_id() << "\n";
    // FP.2a : Implement the function with an infinite loop that measures the time between two loop cycles 
    // and toggles the current phase of the traffic light between red and green and sends an update method 
    // to the message queue using move semantics. The cycle duration should be a random value between 4 and 6 seconds. 
    // Also, the while-loop should use std::this_thread::sleep_for to wait 1ms between two cycles. 

    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int> distribution(4,6);
    double cycleDuration  = distribution(generator); // random value between 4 and 6 seconds.   // FP.2a 

    std::chrono::time_point<std::chrono::system_clock> lastUpdate;
    lastUpdate = std::chrono::system_clock::now();

    TrafficLightPhase phase;
    
    while (true)
    {
        // sleep to reduce CPU utilization
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Compute time since last time we toggled the traffic light  (this will be seconds as cycleDuration is also in sec..)
        long timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - lastUpdate).count();
        if (timeSinceLastUpdate >= cycleDuration)
        {
            //Toggle
            if (_currentPhase ==TrafficLightPhase::red)
            {
                _currentPhase = TrafficLightPhase::green;
            }
            else
            {
                _currentPhase = TrafficLightPhase::red;
            }

            // Everytime the TrafficLight changes phase, it writes to the Message queue
            phase = _currentPhase;
            _queueMgr.send(std::move(phase));   
            
            cycleDuration = distribution(generator);  
            lastUpdate = std::chrono::system_clock::now();
        }
    }
}

