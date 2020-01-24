#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "NetworkRequestChannel.h"

using namespace std;

void *patient_function(BoundedBuffer &buff, mutex &m, int patient, int requests) {
    /* What will the patient threads do? */
    //going to request first {requests} datapoints of {patient} and default {ecg 1}
    double val = requests*.004;
    patient += 1; //try this for patient being 0
    datamsg datmsg(patient,val,1);
    for(int i=0; i<requests; i+=1){//potential no like increment by .004
        datmsg.seconds = i*.004;
        //datmsg is updated now just push into buff
        char* ms = new char[sizeof(datamsg)];
        *(datamsg*)ms = datmsg;
        char* msC = reinterpret_cast<char*>(ms);
        vector<char> msV(msC,msC+sizeof(datamsg));
        buff.push(msV);
        delete[] ms;
        //just example of how i pushed msg into buff previously to copy
        //MESSAGE_TYPE quit_msg = QUIT_MSG;
        //char *quitbuff = new char[sizeof(datamsg)];
        //*(MESSAGE_TYPE *) quitbuff = quit_msg; //MESSAGE TYPE TO DATAMSG
        //char *quitbuffc = reinterpret_cast<char *>(quitbuff);
        //vector<char> quitbuffv(quitbuffc, quitbuffc + sizeof(MESSAGE_TYPE));
        //request_buffer.push(quitbuffv);
    }
    return nullptr;
}

void *worker_function(BoundedBuffer &buff, bool isFileReq, NRC *chan, mutex &m,HistogramCollection& hc,string hostname,string port) {
    /*MESSAGE_TYPE c = NEWCHANNEL_MSG;
    m.lock();
    chan->cwrite(&c, sizeof(MESSAGE_TYPE));
    char *buf = chan->cread();
    m.unlock();*/
    NRC wChan(hostname, port);
    //cout<<buf<<endl;
    bool cond = true;
    while (cond) {
        vector<char> a = buff.pop();
        char *a1 = reinterpret_cast<char *>(a.data());
        MESSAGE_TYPE msg = *(MESSAGE_TYPE *) a1;
        char* readStuff;
        string filename;
        if(msg==QUIT_MSG) {//quitmsg
            buff.push(a);
            cond = false;
        }
        else if(msg==FILE_MSG) {//filemsg
            filemsg fm = *(filemsg*)a1;
            wChan.cwrite(a1, a.size());
            readStuff = wChan.cread();
            //now write to file here
            filename = "./received/y";
            filename += (a1 + sizeof(filemsg));
            //use special mutex?
            m.lock();
            int fd = open(filename.c_str(), O_WRONLY | O_CREAT, 0644);
            lseek(fd,fm.offset,SEEK_SET);
            write(fd,readStuff,fm.length);
            close(fd);
            m.unlock();

        }
        else if(msg==DATA_MSG) {//datamsg
            datamsg dm = *(datamsg*)a1;
            wChan.cwrite(&dm, sizeof(datamsg));
            readStuff = wChan.cread();
            //cout<<*(double*)readStuff<<endl;
           // m.lock();
            hc.update(dm.person-1,*(double*)(readStuff));
           // m.unlock();

        }
    }
    MESSAGE_TYPE q = QUIT_MSG;
    wChan.cwrite((char *) &q, sizeof(MESSAGE_TYPE));
    return nullptr;
}

int main(int argc, char *argv[]) {
    mutex mu;
    int n = 100;    //default number of requests per "patient"
    int p = 10;     // number of patients [1,15]
    int w = 100;    //default number of worker threads
    int b = 20;    // default capacity of the request buffer, you should change this default
    int m = MAX_MESSAGE;    // default capacity of the file buffer
    string port = "";
    string hostname = "";
    string filename;
    srand(time_t(NULL));

    //parse command line args
    int opt;
    while ((opt = getopt(argc, argv, "n:p:w:b:m:f:h:r:")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'm':
                m = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'h':
                hostname = optarg;
                break;
            case 'r':
                port = (optarg);
            default:
                //cout << "invalid option please use command line parameters to specify what action to take" << endl;
                break;
        }
    }

    /*int pid = fork();
    if (pid == 0) {
        // modified to pass along m
        string sm = to_string(m);
        execl("dataserver", "dataserver", sm.c_str(), (char *) NULL);

    }*/

    NRC *chan = new NRC(hostname, port);
    BoundedBuffer request_buffer(b);
    HistogramCollection hc;
    bool isFileReq = (!filename.empty());


    struct timeval start, end;
    gettimeofday(&start, 0);

    /* Start all threads here */
    if (filename.empty()) {
        //making worker and patient threads
        hc.fillHist(p);
        vector<thread> workerVect;
        workerVect.reserve(w);
        for (int i = 0; i < w; i++) {
            workerVect.emplace_back(worker_function, ref(request_buffer), isFileReq, chan,
                                    ref(mu),ref(hc),hostname,port);//worker_functions,args <- add args//give channel
        }
        vector<thread> patientVect;
        patientVect.reserve(p);
        for (int i = 0; i < p; i++) {
            patientVect.emplace_back(patient_function, ref(request_buffer),ref(mu),i,n);//patient_functions,args <- add args
        }
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        //makes sure all patient threads have finished their jobs loading all commands
        for (int i = 0; i < p; i++) {
            if(patientVect[i].joinable())
                patientVect[i].join();
        }
        //then sends quit message to worker threads that will be placed in queue after all patient commands done
        //QUIT STUFF HERE TO STOP WORKER THREADS WITH INFINITE LOOP
        MESSAGE_TYPE quit_msg = QUIT_MSG;
        char *quitbuff = new char[sizeof(datamsg)];
        *(MESSAGE_TYPE *) quitbuff = quit_msg; //MESSAGE TYPE TO DATAMSG
        char *quitbuffc = reinterpret_cast<char *>(quitbuff);
        vector<char> quitbuffv(quitbuffc, quitbuffc + sizeof(MESSAGE_TYPE));
        request_buffer.push(quitbuffv);
        delete[] quitbuff;
        //request_buffer.push(vector<char>((char*)(quit_msg),(char*)(quit_msg) + sizeof(MESSAGE_TYPE)));

        /////////////////////////`//////////////////////////////////////////////////////////////////////////////////////////
        for (int i = 0; i < w; i++) {
            // if(workerVect[i].joinable())
            workerVect[i].join();
        }
    } else {
        //just making worker threads
        vector<thread> workerVect;
        workerVect.reserve(w);
        for (int i = 0; i < w; i++) {
            workerVect.emplace_back(worker_function, ref(request_buffer), isFileReq, chan,
                                    ref(mu),ref(hc),hostname,port);//worker_functions,args <- add args//give channel
        }
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        __int64_t size;
        filemsg fmsg(0, 0);
        //gets file size;
        //need to append string to end of fmsg
        int total_size = sizeof(filemsg) + filename.size() + 1;
        char buff[total_size];
        memcpy(buff, &fmsg, sizeof(filemsg));
        strcpy(buff + sizeof(filemsg), filename.c_str());
        mu.lock();
        chan->cwrite(buff, total_size);
        char *buf = chan->cread();
        mu.unlock();
        //cast buf which will be special __int64_t
        size = *(__int64_t *) (buf); // now holds full file size
        delete buf;
        cout << "file size is: " << size << endl;
       /////////////////////////////////////
        int length = 0;
        int offset = 0;
        if (size >= MAX_MESSAGE) {
            length = MAX_MESSAGE;
        } else {
            length = size;
        }
        while (size > 0) {
            fmsg.offset = offset;
            fmsg.length = length;
            //cout << offset << " " << length << endl;
            //cout << buff << endl;
            memcpy(buff, &fmsg, sizeof(filemsg));
            //now move buffer to vector<char>
            vector<char> pushme(buff,buff+total_size);
            request_buffer.push(pushme);
            size -= length;
            offset += length;
            if (size >= MAX_MESSAGE) {
                length = MAX_MESSAGE;
            } else {//case where size is smaller than max
                length = size;
            }
        }

        /////////////////////////////////////


        //QUIT STUFF HERE TO STOP WORKER THREADS WITH INFINITE LOOP
        MESSAGE_TYPE quit_msg = QUIT_MSG;
        char *quitbuff = new char[sizeof(datamsg)];
        *(MESSAGE_TYPE *) quitbuff = quit_msg; //MESSAGE TYPE TO DATAMSG
        char *quitbuffc = reinterpret_cast<char *>(quitbuff);
        vector<char> quitbuffv(quitbuffc, quitbuffc + sizeof(MESSAGE_TYPE));
        request_buffer.push(quitbuffv);
        delete[] quitbuff;
        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //end join threads
        for (int i = 0; i < w; i++) {
            // if(workerVect[i].joinable());
            workerVect[i].join();
        }
    }

    /* Join all threads at end of each if statement*/
    gettimeofday(&end, 0);
    hc.print();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int) 1e6;
    int usecs = (int) (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micor seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite((char *) &q, sizeof(MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;

}
