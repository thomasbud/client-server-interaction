/*
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 2/8/19
 */
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;

int main(int argc, char *argv[])
{
    int n = 100; // default number of requests per "patient"
    int p = 15;  // number of patients
    srand(time_t(NULL));

    // PART 4 ----------------------------------------------------------------------------------------
    char *arg[2];
    arg[0] = "./dataserver";
    arg[1] = NULL;

    if (fork() == 0)
    {
        //run the server
        execv(arg[0], arg);
    }
    //run the client
    FIFORequestChannel chan("control", FIFORequestChannel::CLIENT_SIDE);

/*
p patient
t time
e which ecg value
f file
c create a server
*/

    // GETOPT ----------------------------------------------------------------------------------------
    int opt;
    bool active[5] = {false, false, false, false, false}; //the order is ptefc

    //more default values
    int patientARG = 1;
    double timeARG = 0;
    int ecgARG = 1;
    string fileARG = "1.csv";

    while ((opt = getopt(argc, argv, "p:t:e:f:c")) != -1)
        switch (opt)
        {
        case 'p': // PATIENT -------------------------------------------------------------------------
        {
            active[0] = true;
            int pass_in = atoi(optarg);
            if (pass_in > 0 && pass_in <= p)
                patientARG = pass_in;
            else
                cout << "Patient number out of range (1 to 15), reverting to default patient" << endl
                     << endl;
            break;
        }
        case 't': // TIME ----------------------------------------------------------------------------
        {
            active[1] = true;
            double pass_in = atof(optarg);
            if (pass_in >= 0 && pass_in <= 59.996)
                if (((int)(pass_in * 1000)) % 4 == 0)
                    timeARG = pass_in;
                else
                    cout << "Invalid time - time has to be a multiple of .004, reverting to default time" << endl
                         << endl;
            else
                cout << "Time is out of range (0 to 59.996), reverting to default time" << endl
                     << endl;
            break;
        }
        case 'e': // ECG VALUE -----------------------------------------------------------------------
        {
            active[2] = true;
            int pass_in = atoi(optarg);
            if (pass_in == 1 || pass_in == 2)
                ecgARG = pass_in;
            else
                cout << "Invalid ECG value - can only be 1 or 2, reverting to default ECG value" << endl
                     << endl;
            break;
        }
        case 'f': // FILE ----------------------------------------------------------------------------
        {
            fileARG = optarg;
            ifstream ifile("./BIMDC/" + fileARG);
            if (ifile)
                active[3] = true;
            else
                cout << "File cannot be found" << endl
                     << endl;
            break;
        }
        case 'c': // CREATE SERVER -------------------------------------------------------------------
        {
            active[4] = true;
            //and that's it
            break;
        }
        }

    // PART 1 ----------------------------------------------------------------------------------------
    if (active[0] && active[1] && active[2]) // if we have P, T, and E arguments
    {
        struct timeval t0, t1;
        gettimeofday(&t0, 0);

        datamsg *x = new datamsg(patientARG, timeARG, ecgARG);
        chan.cwrite(x, sizeof(datamsg));
        char *buf = chan.cread();
        cout << "ECG " << ecgARG << " for Patient " << patientARG << " at time " << timeARG << ": " << *(double *)buf << endl;

        gettimeofday(&t1, 0);
        long long microseconds = (t1.tv_sec - t0.tv_sec) * 1000000LL + (t1.tv_usec - t0.tv_usec);
        long double seconds = (long double)microseconds / 1000000;
        cout << "Process took " << seconds << " seconds" << endl;
    }

    // PART 1 ----------------------------------------------------------------------------------------
    if (active[0] && !active[1] && !active[2]) // if we have P but not T and E arguments, then we read the whole file
    {
        struct timeval t0, t1;
        gettimeofday(&t0, 0);

        ofstream file;
        file.open("./received/x" + to_string(patientARG) + ".csv");
        for (int i = 0; i < 60000; i += 4)
        {
            file << i * 0.001;
            for (int j = 1; j < 3; j++)
            {
                datamsg *x = new datamsg(patientARG, i * 0.001, j);
                chan.cwrite(x, sizeof(datamsg));
                char *buf = chan.cread();
                file << "," << *(double *)buf;
            }
            file << endl;
        }
        file.close();

        gettimeofday(&t1, 0);
        long long microseconds = (t1.tv_sec - t0.tv_sec) * 1000000LL + (t1.tv_usec - t0.tv_usec);
        long double seconds = (long double)microseconds / 1000000;
        cout << "Process took " << seconds << " seconds" << endl;
    }

    // PART 2 ----------------------------------------------------------------------------------------
    if (active[3]) // we want to read a whole file
    {
        struct timeval t0, t1;
        gettimeofday(&t0, 0);
        //string filename = "1.csv";
        filemsg *sizereq = new filemsg(0, 0);
        char *msg = new char[sizeof(filemsg) + fileARG.length() + 1];
        memcpy(msg, sizereq, sizeof(filemsg));
        strcpy(msg + sizeof(filemsg), fileARG.c_str());
        chan.cwrite(msg, sizeof(filemsg) + fileARG.length() + 1);

        char *size = chan.cread();
        __int64_t sizeLeft = *(int *)size;

        ofstream file;
        file.open("./received/y" + fileARG);

        int offsetCount = 0;
        int maxChunk = 256;
        while (sizeLeft > 0)
        {
            if (sizeLeft < maxChunk)
                maxChunk = sizeLeft;
            filemsg *chunk = new filemsg(offsetCount, maxChunk);
            memcpy(msg, chunk, sizeof(filemsg));
            //strcpy(msg + sizeof(filemsg), fileARG.c_str());
            chan.cwrite(msg, sizeof(filemsg) + fileARG.length() + 1);
            char *buf = chan.cread();
            file.write(buf, maxChunk);
            offsetCount += maxChunk;
            sizeLeft -= maxChunk;
        }

        file.close();

        gettimeofday(&t1, 0);
        long long microseconds = (t1.tv_sec - t0.tv_sec) * 1000000LL + (t1.tv_usec - t0.tv_usec);
        long double seconds = (long double)microseconds / 1000000;
        cout << "Process took " << seconds << " seconds" << endl;
    }

    // PART 3 ----------------------------------------------------------------------------------------
    if (active[4]) // we want to make a new server
    {
        struct timeval t0, t1;
        gettimeofday(&t0, 0);

        MESSAGE_TYPE chan_req = NEWCHANNEL_MSG;
        chan.cwrite(&chan_req, sizeof(MESSAGE_TYPE));
        char *chan_name = chan.cread();
        FIFORequestChannel newChan(chan_name, FIFORequestChannel::CLIENT_SIDE);

        gettimeofday(&t1, 0);
        long long microseconds = (t1.tv_sec - t0.tv_sec) * 1000000LL + (t1.tv_usec - t0.tv_usec);
        long double seconds = (long double)microseconds / 1000000;
        cout << "Process took " << seconds << " seconds" << endl;

        //let's try for the first tenth of a second
        ofstream file;
        file.open("./received/newserverout.csv"); //we will print it out to newserverout
        for (int i = 0; i < 100; i += 4)
        {
            file << i * 0.001;
            for (int j = 1; j < 3; j++)
            {
                datamsg *x = new datamsg(1, i * 0.001, j);
                newChan.cwrite(x, sizeof(datamsg));
                char *buf = newChan.cread();
                file << "," << *(double *)buf;
            }
            file << endl;
        }
        file.close();

        // closing the channel
        MESSAGE_TYPE quit_req = QUIT_MSG; // PART 5 ------------------------------------------------------
        newChan.cwrite(&quit_req, sizeof(MESSAGE_TYPE));
    }

    // PART 1 ----------------------------------------------------------------------------------------
    /*
    struct timeval t0, t1;
    gettimeofday(&t0, 0);

    ofstream file;
    file.open("./received/x1.csv");
    for (int i = 0; i < 60000; i += 4)
    {
        file << i * 0.001;
        for (int j = 1; j < 3; j++)
        {
            datamsg *x = new datamsg(1, i * 0.001, j);
            chan.cwrite(x, sizeof(datamsg));
            char *buf = chan.cread();
            file << "," << *(double *)buf;
        }
        file << endl;
    }
    file.close();

    gettimeofday(&t1, 0);
    long long microseconds = (t1.tv_sec - t0.tv_sec)*1000000LL + (t1.tv_usec - t0.tv_usec);
    long double seconds = (long double)microseconds / 1000000;
    cout << "Process took " << seconds << " seconds" << endl;
    */

    // PART 2 ----------------------------------------------------------------------------------------
    /*
    struct timeval t0, t1;
    gettimeofday(&t0, 0);
    string filename = "1.csv";
    filemsg* sizereq = new filemsg(0, 0);
    char* msg = new char[sizeof(filemsg) + filename.length() + 1];
    memcpy(msg, sizereq, sizeof(filemsg));
    strcpy(msg + sizeof(filemsg), filename.c_str());
    chan.cwrite(msg, sizeof(filemsg) + filename.length() + 1);

    char* size = chan.cread();
    __int64_t sizeLeft = *(int*)size;


    ofstream file;
    file.open ("./received/y1.csv");

    int offsetCount = 0;
    int maxChunk = 256;
    while(sizeLeft > 0)
    {
        if (sizeLeft < maxChunk)
            maxChunk = sizeLeft;
        filemsg* chunk = new filemsg(offsetCount, maxChunk);
        memcpy(msg, chunk, sizeof(filemsg));
        //strcpy(msg + sizeof(filemsg), filename.c_str());
        chan.cwrite(msg, sizeof(filemsg) + filename.length() + 1);
        char* buf = chan.cread();
        file << buf;
        offsetCount += maxChunk;
        sizeLeft -= maxChunk;
    }

    file.close();
    
    gettimeofday(&t1, 0);
    long long microseconds = (t1.tv_sec - t0.tv_sec)*1000000LL + (t1.tv_usec - t0.tv_usec);
    long double seconds = (long double)microseconds / 1000000;
    cout << "Process took " << seconds << " seconds" << endl;
    */

    // PART 3 ----------------------------------------------------------------------------------------
    /*
    struct timeval t0, t1;
    gettimeofday(&t0, 0);

    MESSAGE_TYPE chan_req = NEWCHANNEL_MSG;
    chan.cwrite(&chan_req, sizeof(MESSAGE_TYPE));
    char* chan_name = chan.cread();
    FIFORequestChannel newChan (chan_name, FIFORequestChannel::CLIENT_SIDE);

    gettimeofday(&t1, 0);
    long long microseconds = (t1.tv_sec - t0.tv_sec)*1000000LL + (t1.tv_usec - t0.tv_usec);
    long double seconds = (long double)microseconds / 1000000;
    cout << "Process took " << seconds << " seconds" << endl;
    
    
    //let's try for the first tenth of a second
    ofstream file;
    file.open ("./received/1.csv");
    for(int i=0; i<100; i+=4)
    {
        file << i*0.001;
        for(int j=1; j<3; j++)
        {
            datamsg* x = new datamsg(1, i*0.001, j);
            newChan.cwrite (x, sizeof (datamsg));
            char* buf = newChan.cread();
            file << "," << *(double*)buf;
        }
        file << endl;
    }
    file.close();
    
    
    // closing the channel
    MESSAGE_TYPE quit_req = QUIT_MSG; // PART 5 ------------------------------------------------------
    newChan.cwrite (&quit_req, sizeof (MESSAGE_TYPE));
    */

    // PART 4 ----------------------------------------------------------------------------------------
    /*
    char *arg[2];
    arg[0] = "./dataserver";
    arg[1] = NULL;
  
    if(fork()==0)
    {
        //run the server
        execv(arg[0], arg);
    }
    //run the client
    FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
    */

    // closing the channel
    MESSAGE_TYPE quit_req = QUIT_MSG;
    chan.cwrite(&quit_req, sizeof(MESSAGE_TYPE));

    wait(NULL);
}
