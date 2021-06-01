#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
using namespace std;

void patient_function(BoundedBuffer *BB, int patientNum, int requestNum)
{
    /* What will the patient threads do? */
    for (int i = 0; i < requestNum; i++)
    {
        datamsg *msg = new datamsg(patientNum, i * 0.004, 1);
        vector<char> arrayedMsg((char *)msg, (char *)msg + sizeof(datamsg));
        BB->push(arrayedMsg);
        delete msg;
    }
}
void patient_function_file(BoundedBuffer *BB, string filer, __int64_t sizer, int maxer)
{
    int fd = open(("received/y" + filer).c_str(), O_CREAT | O_TRUNC | O_WRONLY | ios::out | ios::binary, 0777);
    lseek(fd, sizer, SEEK_SET);
    close(fd);

    __int64_t sizeLeft = sizer;
    char *msg = new char[sizeof(filemsg) + filer.length() + 1];
    int offsetCount = 0;
    int maxChunk = maxer;
    while (sizeLeft > 0)
    {
        if (sizeLeft < maxChunk)
            maxChunk = sizeLeft;
        filemsg *chunk = new filemsg(offsetCount, maxChunk);
        memcpy(msg, chunk, sizeof(filemsg));
        strcpy(msg + sizeof(filemsg), filer.c_str());
        vector<char> arrayedMsg(msg, msg + sizeof(filemsg) + filer.length() + 1);
        BB->push(arrayedMsg);

        offsetCount += maxChunk;
        sizeLeft -= maxChunk;
        delete chunk;
    }
    delete msg;
}

void worker_function(BoundedBuffer *BB, FIFORequestChannel *workChan, HistogramCollection *hist, string fARG)
{
    /* Functionality of the worker threads */
    int fd = open(("received/y" + fARG).c_str(), O_WRONLY | ios::out | ios::binary, 0777);
    while (true)
    {
        vector<char> vecMsg = BB->pop();
        char *charMsg = reinterpret_cast<char *>(vecMsg.data());
        MESSAGE_TYPE typeMsg = *(MESSAGE_TYPE *)charMsg;
        if (typeMsg == DATA_MSG)
        {
            datamsg *dmsg = (datamsg *)charMsg;
            workChan->cwrite(charMsg, sizeof(datamsg));
            char *buf = workChan->cread();
            double ecg_value = *(double *)buf;
            hist->update(dmsg->person, ecg_value);
            delete buf;
        }
        else if (typeMsg == FILE_MSG)
        {
            //cout << fARG << endl;
            filemsg *fmsg = (filemsg *)charMsg;
            //cout << charMsg + sizeof (filemsg);
            workChan->cwrite(charMsg, sizeof(filemsg) + fARG.length() + 1);
            char *buf = workChan->cread();
            //write to file here
            lseek(fd, fmsg->offset, SEEK_SET);
            write(fd, buf, fmsg->length);
            delete buf;
        }
        else if (typeMsg == QUIT_MSG)
        {
            workChan->cwrite(charMsg, sizeof(MESSAGE_TYPE));
            delete workChan;
            break;
        }
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    int n = 15000;            // default number of requests per "patient"
    int p = 15;               // number of patients [1,15]
    int w = 100;              // default number of worker threads
    int b = 50;               // default capacity of the request buffer, you should change this default
    int m = MAX_MESSAGE;      // default capacity of the file buffer
    string fileARG = "1.csv"; //default filename

    int opt;
    bool activeFile = false;

    while ((opt = getopt(argc, argv, "n:p:w:b:f:")) != -1)
        switch (opt)
        {
        case 'n': // NUMBER OF REQUESTS --------------------------------------------------------------
        {
            int pass_in = atoi(optarg);
            if (pass_in > 0 && pass_in <= 15000)
                n = pass_in;
            else
                cout << "Request amount out of range (1 to 15000), reverting to default amount" << endl
                     << endl;
            break;
        }
        case 'p': // NUMBER OF PATIENTS --------------------------------------------------------------
        {
            int pass_in = atoi(optarg);
            if (pass_in > 0 && pass_in <= 15)
                p = pass_in;
            else
                cout << "Number of patients out of range (1 to 15), reverting to default amount" << endl
                     << endl;
            break;
        }
        case 'w': // NUMBER OF WORKERS ---------------------------------------------------------------
        {
            int pass_in = atoi(optarg);
            if (pass_in > 0)
                w = pass_in;
            else
                cout << "Invalid amount of workers - has to have at least 1 worker, reverting to default amount" << endl
                     << endl;
            break;
        }
        case 'b': // BUFFER CAPACITY -----------------------------------------------------------------
        {
            int pass_in = atoi(optarg);
            if (pass_in > 0)
                b = pass_in;
            else
                cout << "Invalid buffer size - has to have space, reverting to default amount" << endl
                     << endl;
            break;
        }
        case 'f': // FILE ----------------------------------------------------------------------------
        {
            activeFile = true;
            fileARG = optarg;
            break;
        }
        }

    thread patientThreads[p];
    thread patientThread;
    thread workerThreads[w];
    srand(time_t(NULL));

    int pid = fork();
    if (pid == 0)
    {
        // modify this to pass along m
        execl("dataserver", "dataserver", (char *)NULL);
    }

    FIFORequestChannel *chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    HistogramCollection hc;

    for (int i = 0; i < p; i++)
    {
        Histogram *h = new Histogram(10, -2, 2);
        hc.add(h);
    }

    struct timeval start, end;
    gettimeofday(&start, 0);

    if (!activeFile)
    {
        /* Start all threads here */
        for (int i = 0; i < p; i++)
        {
            patientThreads[i] = thread(patient_function, &request_buffer, i + 1, n);
        }

        for (int i = 0; i < w; i++)
        {
            MESSAGE_TYPE chan_req = NEWCHANNEL_MSG;
            chan->cwrite((char *)&chan_req, sizeof(MESSAGE_TYPE));
            char *chan_name = chan->cread();
            FIFORequestChannel *workerChan = new FIFORequestChannel(chan_name, FIFORequestChannel::CLIENT_SIDE);

            workerThreads[i] = thread(worker_function, &request_buffer, workerChan, &hc, fileARG);
            delete chan_name;
        }

        /* Join all threads here */
        for (int i = 0; i < p; i++)
        {
            patientThreads[i].join();
        }

        for (int i = 0; i < w; i++)
        {
            MESSAGE_TYPE quit_req = QUIT_MSG;
            vector<char> arrayedMsg((char *)&quit_req, (char *)&quit_req + sizeof(quit_req));
            request_buffer.push(arrayedMsg);
        }

        for (int i = 0; i < w; i++)
        {
            workerThreads[i].join();
        }
    }
    if (activeFile) ////////// PART 2
    {
        filemsg *sizeMsg = new filemsg(0, 0);
        char *msg = new char[sizeof(filemsg) + fileARG.length() + 1];
        memcpy(msg, sizeMsg, sizeof(filemsg));
        strcpy(msg + sizeof(filemsg), fileARG.c_str());
        chan->cwrite(msg, sizeof(filemsg) + fileARG.length() + 1);

        char *sizeChan = chan->cread();
        __int64_t sizeLeft = *(int *)sizeChan;

        delete sizeMsg;
        delete msg;
        delete sizeChan;

        //////////// Start all threads here
        patientThread = thread(patient_function_file, &request_buffer, fileARG, sizeLeft, m);

        for (int i = 0; i < w; i++)
        {
            MESSAGE_TYPE chan_req = NEWCHANNEL_MSG;
            chan->cwrite((char *)&chan_req, sizeof(MESSAGE_TYPE));
            char *chan_name = chan->cread();
            FIFORequestChannel *workerChan = new FIFORequestChannel(chan_name, FIFORequestChannel::CLIENT_SIDE);

            workerThreads[i] = thread(worker_function, &request_buffer, workerChan, &hc, fileARG);
            delete chan_name;
        }

        ///////////// Join all threads here
        patientThread.join();

        for (int i = 0; i < w; i++)
        {
            MESSAGE_TYPE quit_req = QUIT_MSG;
            vector<char> arrayedMsg((char *)&quit_req, (char *)&quit_req + sizeof(quit_req));
            request_buffer.push(arrayedMsg);
        }

        for (int i = 0; i < w; i++)
        {
            workerThreads[i].join();
        }
    }

    gettimeofday(&end, 0);
    hc.print();
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int)1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int)1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micor seconds" << endl;

    MESSAGE_TYPE q = QUIT_MSG;
    chan->cwrite((char *)&q, sizeof(MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;
}
