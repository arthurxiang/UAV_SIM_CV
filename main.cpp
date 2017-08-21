#include <opencv2/opencv.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>

using namespace cv;
using namespace std;
struct ScreenShot
{
    ScreenShot(int x, int y, int width, int height):
            x(x),
            y(y),
            width(width),
            height(height)
    {
        display = XOpenDisplay(nullptr);
        root = DefaultRootWindow(display);

        init = true;
    }

    void operator() (Mat& cvImg)
    {
        if(init == true)
            init = false;
        else
            XDestroyImage(img);

        img = XGetImage(display, root, x, y, width, height, AllPlanes, ZPixmap);

        cvImg = Mat(height, width, CV_8UC4, img->data);
    }

    ~ScreenShot()
    {
        if(init == false)
            XDestroyImage(img);

        XCloseDisplay(display);
    }

    Display* display;
    Window root;
    int x,y,width,height;
    XImage* img;

    bool init;
};
typedef struct _flightdata
{
    /* imgid
     * tspan
     * pitch
     * roll
     * yaw
     * lat
     * lng
     * alt
     * */
    float pitch;
    float roll;
    float yaw;
    float lat;
    float lng;
    float alt;
}flightdata;
typedef struct _datanode
{
    int32_t  index;
    float data[8];
}datanode;
static flightdata stFlightData;
void * ReadData(void *)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(49006);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }
    unsigned char buff[1024];
    struct sockaddr_in clientAddr1;
    int n;
    int len = sizeof(clientAddr1);
    struct sockaddr_in clientAddr2;
    clientAddr2.sin_family = AF_INET;
    clientAddr2.sin_port = htons(49005);
    clientAddr2.sin_addr.s_addr = inet_addr("127.0.0.1");
    while (1) {
        n = recvfrom(sock, buff, 1021, 0, (struct sockaddr *) &clientAddr1, (socklen_t *) &len);
        if (n > 0) {
            n = sendto(sock, buff, n, 0, (struct sockaddr *) &clientAddr2, sizeof(clientAddr2));
            uint32_t  count=(n-5)/sizeof(datanode);
            for (int i = 0; i <count ; ++i) {
                datanode *pdata = (datanode *) (buff + 5 + i * sizeof(datanode));
                if (pdata->index==130)
                {
                    stFlightData.lng=pdata->data[0];
                    stFlightData.lat=pdata->data[1];
                    stFlightData.alt=pdata->data[2];
                    stFlightData.yaw=pdata->data[3];
                    stFlightData.pitch=pdata->data[4];
                    stFlightData.roll=pdata->data[5];
                }
            }
        } else {

            perror("recv");
            break;
        }
    }
}
int main(int argc, char** argv) {
    if (argc != 2) {
        if (argc != 2) {
            cout << endl << "Usage: ./UAV_SIM_CV path_to_saveImg" << endl;
            return 1;
        }
    }
    string path = argv[1];
    if (path.back() != '/') {
        path += '/';
    }

    pthread_t pid;
    pthread_create(&pid, NULL, &ReadData, NULL);

    struct timeval lasttv;
    gettimeofday(&lasttv, NULL);
    uint64_t lastms = lasttv.tv_sec * 1000 + lasttv.tv_usec / 1000;
    uint32_t imgid = 0;
    FileStorage fs;
    stringstream ss;
    ss << path << "data.xml";
    fs.open(ss.str(), FileStorage::WRITE);
    Mat datanode = Mat::zeros(1, 8, CV_64FC1);
    namedWindow("img");
    while (1) {
        ss.str("");
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint64_t nowms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        int tspan = nowms - lastms;
        lastms = nowms;
        ScreenShot screen(0, 24, 1920, 1080 - 24);
        Mat img;
        screen(img);
        ss << path << "img" << imgid << ".jpg";
        imshow("img", img);
        imwrite(ss.str(), img);
        ss.str("");
        ss << "img" << imgid;
        datanode.at<double>(0, 0) = imgid;
        datanode.at<double>(0, 1) = tspan;
        datanode.at<double>(0, 2) = stFlightData.pitch;
        datanode.at<double>(0, 3) = stFlightData.roll;
        datanode.at<double>(0, 4) = stFlightData.yaw;
        datanode.at<double>(0, 5) = stFlightData.lng;
        datanode.at<double>(0, 6) = stFlightData.lat;
        datanode.at<double>(0, 7) = stFlightData.alt;
        fs << ss.str() << datanode;
        imgid++;
        waitKey(3000);
    }
    return 0;
}