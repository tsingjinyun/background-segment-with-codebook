
/***
平均背景法进行图像分割, 在图像背景相对稳定的情况下, 检测闯入图像的前景目标
*/
/***
	Averaging Background Method
	We’ve just seen a simple method of learning background scenes and segmenting fore-
	ground objects. It will work well only with scenes that do not contain moving background 
	components (like a waving curtain or waving trees). It also assumes that the lighting 
	remains fairly constant (as in indoor static scenes). 
*/
#include "stdafx.h"
#include "cv.h"
#include "highgui.h"

/***************************************************/
//我们为需要的不同临时图像和统计属性的图像创建指针

//Float 3-channel images
IplImage *IavgF, *IdiffF, *IprevF, *IhiF, *IlowF;
IplImage *Iscratch, *Iscratch2;

//Float 1-channel images
IplImage *Igray1, *Igray2, *Igray3;
IplImage *Ilow1, *Ilow2, *Ilow3;
IplImage *Ihi1, *Ihi2, *Ihi3;

//Byte 1-channel image
IplImage *Imaskt;
IplImage *Imask;

float Icount;
/**************************************************/


void AllocateImages(IplImage* I)
//该函数为需要的所有临时图像分配内存,传入来自视频的首帧图像作为大小参考
{
	CvSize sz = cvGetSize(I);

	IavgF	= cvCreateImage(sz, IPL_DEPTH_32F, 3);
	IdiffF	= cvCreateImage(sz, IPL_DEPTH_32F, 3);
	IprevF	= cvCreateImage(sz, IPL_DEPTH_32F, 3);
	IhiF	= cvCreateImage(sz, IPL_DEPTH_32F, 3);
	IlowF	= cvCreateImage(sz, IPL_DEPTH_32F, 3);

	Ilow1	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Ilow2	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Ilow3	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Ihi1	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Ihi2	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Ihi3	= cvCreateImage(sz, IPL_DEPTH_32F, 1);

	cvZero(IavgF);
	cvZero(IdiffF);
	cvZero(IprevF);
	cvZero(IhiF);
	cvZero(IlowF);
	
	Icount = 1e-5;

	Iscratch	= cvCreateImage(sz, IPL_DEPTH_32F, 3);
	Iscratch2	= cvCreateImage(sz, IPL_DEPTH_32F, 3);

	cvZero(Iscratch);
	cvZero(Iscratch2);

	Igray1	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Igray2	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Igray3	= cvCreateImage(sz, IPL_DEPTH_32F, 1);
	Imaskt	= cvCreateImage(sz, IPL_DEPTH_8U, 1);
	Imask	= cvCreateImage(sz, IPL_DEPTH_8U, 1);

}

void accumulateBackground(IplImage* I)
//累积背景图像和前后帧图像差值的绝对值
//当累积够一定数量后就将其转换成一个背景统计模型
{
	static int first = 1;
	//局部静态变量,只初始化一次,意思就是第一次被赋值为1

	cvCvtScale(I, Iscratch, 1, 0);
	//将I指向的图像复制给Iscratch 不能用cvCopy,因为像素的位深度不同

	if (!first)
	{
		cvAcc(Iscratch, IavgF);
		//累积原始的浮点图像到IIavgF
		cvAbsDiff(Iscratch, IprevF, Iscratch2);
		//计算前后帧图像绝对差图像到Iscratch2
		cvAcc(Iscratch2, IdiffF);
		//将前后帧差值图像累加到IdiffF 中
		Icount += 1.0;
		//记录累加的次数用于背景统计时计算均值
	}
	first = 0;
	//first 为局部静态变量,以后调用该函数将不再初始化为1
	//意思就是除了第一次,以后调用该函数均进入if 语句

	cvCopy(Iscratch, IprevF);
	//IprevF用来保存前一帧图像
}

void setHighThreshold(float scale)
{
	cvConvertScale(IdiffF, Iscratch, scale);
	//将统计的绝对差分图像值放大scale 倍赋给Iscratch
	cvAdd(Iscratch, IavgF, IhiF);
	//IhiF = Iscratch + IavgF 
	cvSplit(IhiF, Ihi1, Ihi2, Ihi3, 0);
	//将阀值上限分割为多通道
}

void setLowThreshold(float scale)
{
	cvConvertScale(IdiffF, Iscratch, scale);
	cvSub(IavgF, Iscratch, IlowF);
	//IlowF = IavgF - Iscratch
	cvSplit(IlowF, Ilow1, Ilow2, Ilow3, 0);
	//将阀值下限分割为多通道
}

void createModelsfromStats()
//当累积足够多的帧图后,就将其转化成一个背景统计模型
//该函数用于计算每个像素的均值和平均绝对差分
{
	cvConvertScale(IavgF, IavgF, (double)(1.0/Icount));
	//计算平均原始图像到 IavgF
	cvConvertScale(IdiffF, IdiffF, (double)(1.0/Icount));
	//计算绝对差分图像到 IdiffF

	cvAddS(IdiffF, cvScalar(1.0, 1.0, 1.0), IdiffF);
	//使得到的绝对差分图像每个像素值均不为空

	setHighThreshold(7.0);//使得对于每一帧图像的绝对差大于平均值7倍的像素都被认为是前景
	setLowThreshold(6.0);//使得对于每一帧图像的绝对差小于平均值6倍的像素都被认为是前景
	//根据统计的背景模型设定一个阀值上限和下限
	//如果 IlowF <= Temp < IhiF 时认为其为背景,否则为视频中出现的运动目标物体
}


//图像分割
void backgroundDiff(IplImage* I)
{
	cvCvtScale(I, Iscratch, 1, 0);
	//转换成浮点型图像：将I指向的图像复制给Iscratch 不能用cvCopy, 因为像素的位深度不同
	cvSplit(Iscratch, Igray1, Igray2, Igray3, 0);
	//得到的当前帧分割成3个单通道图像
	cvInRange(Igray1, Ilow1, Ihi1, Imask);
	//        src     lower  upper dst
	//检查这些单通道图像是否在平均背景像素高低阀值之间
	//如果src(I)在范围内(lower <= src < upper)dst(I)被设置为0xff(每一位都是 '1')否则置0
	cvInRange(Igray2, Ilow2, Ihi2, Imaskt);
	cvOr(Imask, Imaskt, Imask);
	//计算两个数组每个元素的按位或值赋值给第三个参数
	cvInRange(Igray3, Ilow3, Ihi3, Imaskt);
	cvOr(Imask, Imaskt, Imask);
	//最后Imask 为分离出的前景二值图
	cvSubRS(Imask, cvScalar(255), Imask);
	//计算数量和数组之间的差,将Imask反相处理
}

void DeallocateImages()
//解除分配的内存
{
	cvReleaseImage(&IavgF);
	cvReleaseImage(&IdiffF);
	cvReleaseImage(&IprevF);
	cvReleaseImage(&IhiF);
	cvReleaseImage(&IlowF);
	cvReleaseImage(&Ilow1);
	cvReleaseImage(&Ilow2);
	cvReleaseImage(&Ilow3);
	cvReleaseImage(&Ihi1);
	cvReleaseImage(&Ihi2);
	cvReleaseImage(&Ihi3);
	cvReleaseImage(&Iscratch);
	cvReleaseImage(&Iscratch2);
	cvReleaseImage(&Igray1);
	cvReleaseImage(&Igray2);
	cvReleaseImage(&Igray3);
	cvReleaseImage(&Imaskt);
	cvReleaseImage(&Imask);
}

int main()
{
	CvCapture* capture = cvCreateFileCapture("D:\\vs程序\\vs2010\\第9章\\616.MP4");
	//初始化从文件中获取视频
	if (!capture)
	{
		printf("Couldn't Open the file.");
		return -1;
	}
	
	cvNamedWindow("raw");
	cvNamedWindow("avg");

	IplImage* rawImage = cvQueryFrame(capture);
	//这个函数仅仅是函数cvGrabFrame和函数cvRetrieveFrame在一起调用的组合
	cvShowImage("raw", rawImage);
	
	AllocateImages(rawImage);//调用子函数
	
	for (int i=0;;i++)
	{
		if (i <= 30) 
		{
			accumulateBackground(rawImage);
			//前30帧用于累积计算背景图像
			if (i == 30)
				//将前30真转换成一个背景统计模型
				createModelsfromStats();
		}
		else 
			//建立好背景模型后调用此函数进行图像分割
			backgroundDiff(rawImage);

		cvShowImage("avg", Imask);
		//播放分割后的目标图像结果

		if (cvWaitKey(33) == 27)
			//每33ms 播放一帧
			break;

		if (!(rawImage = cvQueryFrame(capture)))
			break;
		cvShowImage("raw", rawImage);
		//显示原图像

		if (i == 56 || i == 63)
			//56帧和63帧时暂停
			cvWaitKey();
	}	

	DeallocateImages();
	return 0;
}