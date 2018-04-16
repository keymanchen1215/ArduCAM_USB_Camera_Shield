//Arducam SDK multi-thread example for MT9M001 camera
//This demo is only demonstrate how to use the Arducam SDK functions, 
//Tested under Raspberry Pi3 Raspbian OS Opencv 2.4.13
//Compile with command: g++ MT9M001_demo.cpp -o Test `pkg-config --cflags --libs opencv` -lArduCamLib -lpthread -lusb-1.0 -L. -I. -std=gnu++11
//Copy libArduCamLib.so libusb-1.0.so to /lib folder
//Running the camera with command: sudo ./Test


#include <opencv2/opencv.hpp>
#include "ArduCamLib.h"
#include <unistd.h>  
#include <iostream>
#include <pthread.h>
#include <thread>
#include <termios.h>

using namespace std;
using namespace cv;

ArduCamHandle useHandle;
ArduCamCfg useCfg;
Uint32 pUsbIdxArray[10];

pthread_t ntid1;
pthread_t ntid2;

int running = 0;
char key = 0;
int save_image = 0;

Mat BytestoMat(Uint8* bytes,int width,int height)
{
    Mat image = Mat(height,width,CV_8UC1,bytes);
    return image;
}

//Capture Thread 
void *fnCapture_Thread(void *para)
{ 

    //1. Call beginCapture to start a capture task
    ArduCam_beginCapture(useHandle);
    while(running)
    {
	//2. Continue with capture task
	ArduCam_capture(useHandle);
	//cout << "                         capture thread" << endl;
    }
    //3. Call endCapture to stop and delete the capture task
    ArduCam_endCapture(useHandle );

    pthread_exit(NULL);  
}

//Display Thread 
//Add your image processing here
void *fnDisplay_Thread(void *para)
{
    Uint8* pu8FrameData;
    Mat raw_image;
    Mat rgb_image;
    Mat resize_image;
    char file_name[64];
    int count = 0;
    while(running)
    {
	//1. Check if there is any available frame to read
	if ( ArduCam_available(useHandle) > 0)
	{
	        cout << "frame available\n";
	        //2. Read a frame
	        ArduCam_read(useHandle, pu8FrameData );
	        
	        //3. Display the frame
	        raw_image = BytestoMat(pu8FrameData, 1280, 1024);
	        if ( !raw_image.data )
		{
			cout << "No image data \n";
			//return -1;
		}
		//cout << image.data[100] << image.data[101] << "\n";
		namedWindow("Display Image", WINDOW_AUTOSIZE );
		
		//Select one of the following format to convert
		//CV_BayerBG2RGB =CV_BayerRG2BGR,  
		//CV_BayerGB2RGB =CV_BayerGR2BGR,  
		//CV_BayerRG2RGB =CV_BayerBG2BGR,  
		//CV_BayerGR2RGB =CV_BayerGB2BGR, 

		cvtColor(raw_image, rgb_image, CV_BayerGB2BGR);
		resize(rgb_image, resize_image, cv::Size(640, 480), (0,0), (0,0), cv::INTER_NEAREST);
		imshow("Display Image", resize_image);
		cvWaitKey(60);

		if(save_image)
		{
		    sprintf(file_name, "image_%d.jpg", count++);
		    imwrite(file_name, rgb_image);
		    save_image = 0;
		}

	        //4. Delete the old frame
	        ArduCam_del(useHandle);
	        cout << "Delete frame \n";
	}
    	//cout << "display thread" << endl;
    //usleep(100);
    }
    pthread_exit(NULL);  
}





int main(int argc, char** argv )
{
    unsigned int u32Value = 0xFFFFFFFF;
    bool open_flag = 0;
    
    int number = 0;
    int i ;
    int ret;
    unsigned char VRCMD_Val[3] = { 0x03, 0x04, 0x0C};
    unsigned int size;
    static struct termios oldt, newt;

    tcgetattr( STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON);          

    tcsetattr( STDIN_FILENO, TCSANOW, &newt);
	  
    //1. Scan the camera for index number for manual open API		  
    number = ArduCam_scan( pUsbIdxArray, 0x52cb );
    cout << "Device found = " << number << " \n";
	  
	  
    useCfg.u16Vid = 0x52cb;
    useCfg.u32Width = 1280;
    useCfg.u32Height = 1024;
    useCfg.u8PixelBytes = 1;
    useCfg.u8PixelBits = 8;
    useCfg.emI2cMode = I2C_MODE_8_16;
    useCfg.u32SensorShipAddr = 0xBA;
    
    //2. Open the camera to get the handle using auto open API
    u32Value = ArduCam_autoopen( useHandle, &useCfg );
    //u32Value = ArduCam_open( useHandle, &useCfg, 0 );

    //3. Download the sensor register settings
    ArduCam_writeReg_8_8( useHandle, 0x46, 1, 0x25);
    ArduCam_writeReg_8_8( useHandle, 0x46, 1, 0x05);
    ArduCam_VRCMD(useHandle, 0xF6, 0, 0, 0, 3, VRCMD_Val, &size);
    //Change MT9M001 Gains
    //ArduCam_writeReg_8_16( useHandle, 0xBA, 0x35, 0x61);	//Global Gain
    ArduCam_writeReg_8_16( useHandle, 0xBA, 0x2B, 0x51);
    ArduCam_writeReg_8_16( useHandle, 0xBA, 0x2C, 0x61);
    ArduCam_writeReg_8_16( useHandle, 0xBA, 0x2D, 0x61);
    ArduCam_writeReg_8_16( useHandle, 0xBA, 0x2E, 0x51);

    //4. Create multi-threads for capture and processing
    if (u32Value == 0)
    {
        open_flag = 1;
        cout << "ArduCam_open successful" << endl ;
	running = 1;
    	ret = pthread_create(&ntid1,NULL,fnCapture_Thread,NULL);
	if(ret != 0)
	{
	  cout << "can't create capture thread:" << ret << endl;
	  return -1;
	}
	else
	  cout << "create capture thread successfully" << endl;

    	ret = pthread_create(&ntid2,NULL,fnDisplay_Thread,NULL);
	if(ret != 0)
	{
	  cout << "can't create display thread:" << ret << endl;
	  return -1;
	}
	else
	  cout << "create display thread successfully" << endl;
    }
    else
    {
        open_flag = 0;
        cout << "ArduCam_open failed" << endl;
	return -1;
    }


    
    //5. Waiting for user command
    while(open_flag == 1)
    {
	//cout << "main thread " << endl;
	usleep(1000);    
	cin >> key;
	if(key == 'q')
	{
	    running = 0;
	    break;
	}
	else if(key == 's')
	    save_image = 1;    
	   

    }
    cout << "ArduCam closed" << endl;
    //6. Close the camera handle
    ArduCam_close(useHandle);
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
      
}



