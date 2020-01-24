#ifndef BoundedBuffer_h
#define BoundedBuffer_h

#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

class BoundedBuffer
{
private:
  	int cap;
  	queue<vector<char>> q;

	/* mutex to protect the queue from simultaneous producer accesses
	or simultaneous consumer accesses */
	mutex mtx;
	
	/* condition that tells the consumers that some data is there */
	condition_variable data_available;
	/* condition that tells the producers that there is some slot available */
	condition_variable slot_available;

public:
	BoundedBuffer(int _cap){
    cap = _cap;

	}
	~BoundedBuffer(){
        //potentially need to delete mutex
	}

	void push(vector<char> data){
	    //putting item on queue
        unique_lock<mutex> lck(mtx);
        //need to wait for open slot available
        slot_available.wait(lck,[this]{return q.size()<cap;});

        q.push(data);
        data_available.notify_one();
        return;
	}

	vector<char> pop(){
		vector<char> temp;
        unique_lock<mutex> lck(mtx);
        //need to wait for data available
        data_available.wait(lck,[this]{return q.size()>0;});

		temp = q.front();
		q.pop();
		slot_available.notify_one();
		return temp;

	}
};

#endif /* BoundedBuffer_ */
