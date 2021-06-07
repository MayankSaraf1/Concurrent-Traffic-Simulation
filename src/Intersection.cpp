#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <random>

#include "Street.h"
#include "Intersection.h"
#include "Vehicle.h"

/* Implementation of class "WaitingVehicles" */

int WaitingVehicles::getSize()
{
    std::lock_guard<std::mutex> lock(_mutex);

    return _vehicles.size();
}

void WaitingVehicles::pushBack(std::shared_ptr<Vehicle> vehicle, std::promise<void> &&promise)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _vehicles.push_back(vehicle);
    _promises.push_back(std::move(promise));
}

void WaitingVehicles::permitEntryToFirstInQueue()
{
    std::lock_guard<std::mutex> lock(_mutex);

    // get entries from the front of both queues
    auto firstPromise = _promises.begin();
    auto firstVehicle = _vehicles.begin();

    // fulfill promise and send signal back that permission to enter has been granted
    // msaraf - as soon as this is set.. Intersection::addVehicleToQueue -->> ftrVehicleAllowedToEnter.wait(); will return! IMPORTANT !!
    firstPromise->set_value();

    // remove front elements from both queues
    _vehicles.erase(firstVehicle);
    _promises.erase(firstPromise);
}

/* Implementation of class "Intersection" */

Intersection::Intersection()
{
    _type = ObjectType::objectIntersection;
    _isBlocked = false;
}

void Intersection::addStreet(std::shared_ptr<Street> street)
{
    _streets.push_back(street);
}

std::vector<std::shared_ptr<Street>> Intersection::queryStreets(std::shared_ptr<Street> incoming)
{
    // store all outgoing streets in a vector ...
    std::vector<std::shared_ptr<Street>> outgoings;
    for (auto it : _streets)
    {
        if (incoming->getID() != it->getID()) // ... except the street making the inquiry
        {
            outgoings.push_back(it);
        }
    }

    return outgoings;
}

// adds a new vehicle to the queue and returns once the vehicle is allowed to enter
void Intersection::addVehicleToQueue(std::shared_ptr<Vehicle> vehicle)
{
    // msaraf - locking as we are going to use "cout" which is a shared resource.. 
    std::unique_lock<std::mutex> lck(_mtx);
    std::cout << "Intersection #" << _id << "::addVehicleToQueue: Vehicle #" << vehicle->getID() << std::endl;
    lck.unlock();

    // add new vehicle to the end of the waiting line
    std::promise<void> prmsVehicleAllowedToEnter;
    std::future<void> ftrVehicleAllowedToEnter = prmsVehicleAllowedToEnter.get_future();
    _waitingVehicles.pushBack(vehicle, std::move(prmsVehicleAllowedToEnter));

    // wait until the vehicle is allowed to enter the intersection
    ftrVehicleAllowedToEnter.wait();
    lck.lock();

    // msaraf - At this stage, the Intersection has permitted the Vehicle to enter the interseection meaning
    // the Vehicle has reached the front of the Waiting queue and has been removed from the wait.
    // The implemenation of adding to the Wating queue is inside Intersection::pushBack() function
    // The implemenation of repeatedly checking the Waiting queue is inside Intersection::processVehicleQueue() function
    // - For each intersection, there is a infinitte while loop to check if intersection is not blocked and there is atleast 1 Vehicle in the  Waiting queue, 
    // if so then the intersection::permitEntryToFirstInQueue erase the 1st Vehicle and set_value on the future which resutrns above!

    // FP.6b : use the methods TrafficLight::getCurrentPhase and TrafficLight::waitForGreen to block the execution until the traffic light turns green.

    // msaraf - Now th only thing that is remaining is that the associated Traffic light for this intersection should be green.
    // We need to block further execution until the associated traffi light turns green.

 
    while(_trafficLight.getCurrentPhase() != TrafficLightPhase::green) // FP.6b
    {
        //std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "Intersection #" << _id << ": Vehicle #" << vehicle->getID() << " is first in the queue. Waiting for traffic signal to turn green." << std::endl;

        _trafficLight.waitForGreen();
    }

    std::cout << "Intersection #" << _id << " Traffic Light is green." << " Vehicle #" << vehicle->getID() << " is granted entry." << std::endl;
    lck.unlock();
}

void Intersection::vehicleHasLeft(std::shared_ptr<Vehicle> vehicle)
{
    //std::cout << "Intersection #" << _id << ": Vehicle #" << vehicle->getID() << " has left." << std::endl;

    // unblock queue processing
    this->setIsBlocked(false);
}

void Intersection::setIsBlocked(bool isBlocked)
{
    _isBlocked = isBlocked;
    //std::cout << "Intersection #" << _id << " isBlocked=" << isBlocked << std::endl;
}

// virtual function which is executed in a thread
void Intersection::simulate() // using threads + promises/futures + exceptions
{
    // FP.6a : In Intersection.h, add a private member _trafficLight of type TrafficLight. At this position, start the simulation of _trafficLight.
    _trafficLight.simulate();

    // launch vehicle queue processing in a thread
    threads.emplace_back(std::thread(&Intersection::processVehicleQueue, this));
}

void Intersection::processVehicleQueue()
{
    // print id of the current thread
    //std::cout << "Intersection #" << _id << "::processVehicleQueue: thread id = " << std::this_thread::get_id() << std::endl;

    // continuously process the vehicle queue
    while (true)
    {
        // sleep at every iteration to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // only proceed when at least one vehicle is waiting in the queue
        if (_waitingVehicles.getSize() > 0 && !_isBlocked)
        {
            // set intersection to "blocked" to prevent other vehicles from entering
            this->setIsBlocked(true);

            // permit entry to first vehicle in the queue (FIFO)
            _waitingVehicles.permitEntryToFirstInQueue();
        }
    }
}

bool Intersection::trafficLightIsGreen()
{
   // msaraf - shouldn't _currentPhase CS (and hence protected by mutex) as it gets toggled by a different thread running the TrafficLight?
   if (_trafficLight.getCurrentPhase() == TrafficLightPhase::green)
       return true;
   else
       return false;
  
} 