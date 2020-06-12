// Drawing.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
//

#include "stdafx.h"

#include "CblobLabeling.h"
#include <stdio.h>
#include <ctype.h>
#include <windows.h>
#include <opencv2\opencv.hpp>

#include <tchar.h>

IplImage *image = 0, *hsv = 0, *hue = 0, *mask = 0, *backproject = 0, *histimg = 0,
*mask_bp = 0, *mask_han = 0, *hand = 0;

IplImage* frame = 0, *sub_han = 0, *circle = 0;

IplImage *av = 0, *sgm = 0;
IplImage *lower = 0, *upper = 0, *tmp = 0;
IplImage *dst = 0, *msk = 0;

//inPaint ���� ����
IplImage *draw_msk, *col_msk, *col_msk_n, *draw_col;
IplImage *r_col, *g_col, *b_col;

//camshift, convex hull, contours, labeling �� �ʿ��� IplImage ����ü ����
IplImage *convex = 0, *contour = 0, *smal_labl = 0, *cont_labl = 0, *hand_col = 0;
IplImage *movie = 0, *movie_b = 0;

CvMemStorage *storage, *storage_in;
CvPoint2D32f ptSource[4], ptPerspective[4];
CvSeq *contours, *hull, *ptseq, *fin_seq;

CvScalar d_color = CV_RGB(0, 0, 0);

CvHistogram *hist = 0;
CvCapture *capture = 0, *video = 0;
CvVideoWriter *videoOut = 0;

int backproject_mode = 0;
int select_object = 0;
int track_object = 0;
int show_hist = 1;
int fst_label = 1;
int dis_two = 1;

double theta = 0, angle = 0;//���� ���Ⱒ, �� �հ��� ���̰�

CvPoint corner;	CvPoint end_pt;
CvPoint corner2 = cvPoint(0, 0);
CvPoint cen_pt, sub_cen_pt, fin_pt;
CvPoint origin;
CvPoint pt, pt0;

CvRect selection;
CvRect track_window;
CvBox2D track_box;
CvConnectedComp track_comp;

int hdims = 16;
float hranges_arr[] = { 0, 180 };
float* hranges = hranges_arr;
int vmin = 10, vmax = 256, smin = 30;

//�������� �߽��� �ʱⰪ
POINT point_s;

CvScalar hsv2rgb(float hue)
{
	int rgb[3], p, sector;
	static const int sector_data[][3] =
	{ { 0, 2, 1 }, { 1, 2, 0 }, { 1, 0, 2 }, { 2, 0, 1 }, { 2, 1, 0 }, { 0, 1, 2 } };
	hue *= 0.033333333333333333333333333333333f;
	sector = cvFloor(hue);
	p = cvRound(255 * (hue - sector));
	p ^= sector & 1 ? 255 : 0;

	rgb[sector_data[sector][0]] = 255;
	rgb[sector_data[sector][1]] = 0;
	rgb[sector_data[sector][2]] = p;

	return cvScalar(rgb[2], rgb[1], rgb[0], 0);
}

int _tmain(int argc, _TCHAR* argv[])
{
	//	capture = cvCaptureFromFile("c:/test/QCAV/grip_0410a.avi");
	capture = cvCaptureFromCAM(0);
	cvGrabFrame(capture);
	frame = cvQueryFrame(capture);
	cvFlip(frame, frame, 1);

	//	videoOut = cvCreateVideoWriter("c:/test/grip_0410b.avi",-1,30,cvGetSize(frame),1);

	//   cvNamedWindow( "Histogram", 1 );
	cvNamedWindow("CamShiftDemo", 1);

	cvNamedWindow("frame", 1);
	//cvNamedWindow( "hand",0);
	//cvResizeWindow("hand",320,240);

	cvCreateTrackbar("Vmin", "CamShiftDemo", &vmin, 256, 0);
	cvCreateTrackbar("Vmax", "CamShiftDemo", &vmax, 256, 0);
	cvCreateTrackbar("Smin", "CamShiftDemo", &smin, 256, 0);

	image = cvCreateImage(cvGetSize(frame), 8, 3);
	image->origin = frame->origin;
	hsv = cvCreateImage(cvGetSize(frame), 8, 3);
	hue = cvCreateImage(cvGetSize(frame), 8, 1);
	mask = cvCreateImage(cvGetSize(frame), 8, 1);
	backproject = cvCreateImage(cvGetSize(frame), 8, 1);
	mask_bp = cvCreateImage(cvGetSize(frame), 8, 1);
	mask_han = cvCreateImage(cvGetSize(frame), 8, 1);
	hand = cvCreateImage(cvGetSize(frame), 8, 3);
	col_msk = cvCreateImage(cvGetSize(frame), 8, 3);
	col_msk_n = cvCreateImage(cvGetSize(frame), 8, 1);
	draw_col = cvCreateImage(cvGetSize(frame), 8, 3);
	hand_col = cvCreateImage(cvGetSize(frame), 8, 3);


	hist = cvCreateHist(1, &hdims, CV_HIST_ARRAY, &hranges, 1);//hdims = 16
	histimg = cvCreateImage(cvSize(320, 200), 8, 3);
	cvZero(histimg);

	av = cvCreateImage(cvGetSize(frame), IPL_DEPTH_32F, 3);// ���� �Լ��� ������ �ȵ�..why? ����ü ����� cvCreateImage() ����.
	sgm = cvCreateImage(cvGetSize(frame), 32, 3);
	lower = cvCreateImage(cvGetSize(frame), 32, 3);
	upper = cvCreateImage(cvGetSize(frame), 32, 3);
	tmp = cvCreateImage(cvGetSize(frame), 32, 3);
	dst = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 3);
	msk = cvCreateImage(cvGetSize(frame), 8, 1);

	//inPaint
	draw_msk = cvCreateImage(cvGetSize(frame), 8, 1);
	cvZero(draw_msk);
	cvZero(draw_col);

	int i;	int INIT_TIME = 10;

	cvSetZero(av);
	for (i = 0; i < INIT_TIME; i++){
		cvGrabFrame(capture);
		frame = cvQueryFrame(capture);
		cvFlip(frame, frame, 1);
		cvAcc(frame, av);
	}
	cvConvertScale(av, av, 1.0 / INIT_TIME);

	cvSetZero(sgm);
	for (i = 0; i < INIT_TIME; i++){
		cvGrabFrame(capture);
		frame = cvQueryFrame(capture);
		cvFlip(frame, frame, 1);
		cvConvert(frame, tmp);
		cvSub(tmp, av, tmp);
		cvPow(tmp, tmp, 2.0);// ����
		cvConvertScale(tmp, tmp, 2.0);
		cvPow(tmp, tmp, 0.5);//��Ʈ
		cvAcc(tmp, sgm);
	}
	cvConvertScale(sgm, sgm, 1.0 / INIT_TIME);

	//inPaint ��ǥ �ʱ�ȭ
	point_s.x = 0; point_s.y = 0;

	double t;
	int count = 0;
	char file[200];

	FILE *fp, *fp1;
	//fp = fopen("c:/test/qcav/fin_prin_0411h.txt", "wt");
	//fp1 = fopen("c:/test/1229_position_y_noBG.txt","wt");

	for (;;){

		count++;
		//sprintf_s(file,"c:/test/qcav/%d.jpg",count);

		t = (double)cvGetTickCount();

		frame = cvQueryFrame(capture);
		cvFlip(frame, frame, 1);

		//��������Ʈ �Ķ���� �� ����
		double B_PARAM = 1.0 / 40.0;
		double T_PARAM = 1.0 / 120.0;
		double zeta = 25.0;

		cvConvert(frame, tmp);

		cvSub(av, sgm, lower);
		cvSubS(lower, cvScalarAll(zeta), lower);
		cvAdd(av, sgm, upper);
		cvAddS(upper, cvScalarAll(zeta), upper);
		cvInRange(tmp, lower, upper, msk);//����ũ ���� �����

		cvSub(tmp, av, tmp);
		cvPow(tmp, tmp, 2.0);//����
		cvConvertScale(tmp, tmp, 2.0);
		cvPow(tmp, tmp, 0.5);//��Ʈ
		cvRunningAvg(frame, av, B_PARAM, msk);	//����ũ ������ �̿��� �ش� ����(��ü�� ������ ���)�� ������Ʈ(����κ�)
		cvRunningAvg(tmp, sgm, B_PARAM, msk);

		cvNot(msk, msk);
		//		cvRunningAvg(tmp, sgm, T_PARAM, msk);

		cvErode(msk, msk, 0, 1);
		cvErode(msk, msk, 0, 1);
		cvDilate(msk, msk, 0, 1);
		cvDilate(msk, msk, 0, 1);

		cvZero(col_msk);
		cvCvtColor(msk, col_msk, 8);

		if (fst_label){

			IplImage *skin = cvCreateImage(cvGetSize(msk), 8, 1);
			cvCopy(msk, skin);

			cvThreshold(skin, skin, 128, 255, 0);

			CBlobLabeling blob;
			blob.SetParam(skin, 5);
			blob.DoLabeling();

			blob.BlobSmallSizeConstraint(50, 50);
			blob.BlobBigSizeConstraint(350, 480);

			int sum_x = 0, sum_y = 0, num = 0;

			for (int k = 0; k < blob.m_nBlobs; k++){//	int	m_nBlobs; ���̺��� ����

				//printf("%d ", blob.m_nBlobs);

				CvPoint pt1 = cvPoint(blob.m_recBlobs[k].x,   //m_recBlobs; �� ���̺� ����
					blob.m_recBlobs[k].y);

				CvPoint pt2 = cvPoint(pt1.x + blob.m_recBlobs[k].width,
					pt1.y + blob.m_recBlobs[k].height);
				CvPoint pt2a = cvPoint(pt1.x + blob.m_recBlobs[k].width,
					pt1.y + (int)(blob.m_recBlobs[k].width * 1.3));

				CvRect hand, hand_n;
				hand.x = 0; hand.y = 0; hand.width = 0; hand.height = 0;
				hand.x = blob.m_recBlobs[k].x;
				hand.y = blob.m_recBlobs[k].y;
				hand.width = blob.m_recBlobs[k].width;
				hand.height = blob.m_recBlobs[k].height;

				hand_n = hand;
				hand_n.height = (int)(blob.m_recBlobs[k].width * 1.3);

				cvSetImageROI(skin, hand_n);
				IplImage *sub_skin = cvCreateImage(cvSize(skin->roi->width, skin->roi->height), 8, 1);
				cvCopy(skin, sub_skin, 0);
				cvResetImageROI(skin);

				CvPoint pt3 = cvPoint(pt1.x + hand.width, pt1.y + hand.height);// ���� ������ ���̺� �簢 ������ ����

				sum_x = 0, sum_y = 0, num = 0;

				uchar* data_sub = (uchar*)sub_skin->imageData;
				int sub_w = sub_skin->width;
				int sub_h = sub_skin->height;
				int sub_ws = sub_skin->widthStep;

				for (int j = 0; j < sub_h; j++)
				for (int i = 0; i < sub_w; i++){
					if (data_sub[j*sub_ws + i] == 255){
						sum_x = sum_x + i;
						sum_y = sum_y + j;
						num++;
					}
				}

				CvPoint fst;
				for (int j = 0; j < sub_h; j++)
				for (int i = 0; i < sub_w; i++){
					if (data_sub[j*sub_ws + i] == 255){
						fst.x = i;
						fst.y = j;
						i = sub_w; j = sub_h;
					}
				}

				cen_pt.x = pt1.x + cvRound(sum_x / num);
				cen_pt.y = pt1.y + cvRound(sum_y / num);

				sub_cen_pt.x = cvRound(sum_x / num);
				sub_cen_pt.y = cvRound(sum_y / num);

				//convexhull�� �̿��� ���� ã��
				storage = cvCreateMemStorage(0);
				ptseq = cvCreateSeq(CV_SEQ_KIND_GENERIC | CV_32SC2, sizeof(CvContour), sizeof(CvPoint), storage);
				CvSeq* hull;

				for (int j = 0; j < sub_h; j++)  //Dst�� ����ó�� �� �̹���
				for (int i = 0; i < sub_w; i++){
					if (data_sub[j*sub_ws + i] == 255){
						pt0.x = i;
						pt0.y = j;
						cvSeqPush(ptseq, &pt0);
					}
				}

				pt0 = cvPoint(0, 0);// �ܰ��� �׸��� �ʿ��� ��ǥ �ʱ�ȭ

				double fMaxDist = 0.0;

				if (ptseq->total != 0){//ptseq ���� 0�� ��� cvConvexHull2()�Լ� ��� ���ϹǷ� ������ ����.

					hull = cvConvexHull2(ptseq, 0, CV_COUNTER_CLOCKWISE, 0);

					for (int x = 0; x < hull->total; x++){
						//�ܰ��� �׸���
						CvPoint hull_pt = **CV_GET_SEQ_ELEM(CvPoint*, hull, x);

						if (pt0.x == 0 && pt0.y == 0){//�ʱⰪ ����
							pt0 = hull_pt;	end_pt = pt0;
						}
						//				cvLine(hand_col, pt0, hull_pt, CV_RGB(255, 0, 0),2,8);	
						pt0 = hull_pt;

						//������ ������ �ձ�
						//if(x==hull->total-1)
						//	cvLine(hand_col,hull_pt,end_pt,CV_RGB(255,0,0),2,8);

						//���� ��ǥ ���� �� �ִ�Ÿ� ���ϱ�
						CvPoint cor_pt = **CV_GET_SEQ_ELEM(CvPoint*, hull, x);
						double fDist = sqrt((double)((sub_cen_pt.x - cor_pt.x) * (sub_cen_pt.x - cor_pt.x)
							+ (sub_cen_pt.y - cor_pt.y) * (sub_cen_pt.y - cor_pt.y)));

						if (fDist > fMaxDist){
							fin_pt = cor_pt;
							fMaxDist = fDist;
						}
					}
				}

				int rad = (int)fMaxDist;

				IplImage* circle = cvCreateImage(cvGetSize(sub_skin), 8, 1);
				cvSetZero(circle);
				cvCircle(circle, cvPoint((int)(sum_x / num), (int)(sum_y / num)),
					(int)(rad*0.7), CV_RGB(255, 255, 255), 6);

				cvAnd(sub_skin, circle, sub_skin, 0);

				IplImage* finger = cvCreateImage(cvSize(320, 240), 8, 1);
				uchar* n_data = (uchar*)finger->imageData;

				int w, z;

				for (int j = 0; j < 240; j++)
				for (int i = 0; i < 320; i++){

					w = (int)sub_w*i / 320;
					z = (int)sub_h*j / 240;

					n_data[j*finger->widthStep + i] = data_sub[z*sub_skin->widthStep + w];
				}

				CBlobLabeling fin;
				fin.SetParam(finger, 50);
				fin.DoLabeling();


				if (fin.m_nBlobs == 6){

					//				count++;
					fst_label = 0;
					track_object = -1;
					selection = hand;

					cvSetImageROI(frame, hand);
					IplImage *col_han = cvCreateImage(cvSize(frame->roi->width, frame->roi->height), 8, 3);
					cvCopy(frame, col_han);
					cvResetImageROI(frame);

					cvSetImageROI(col_msk, hand);
					IplImage *msk_han = cvCreateImage(cvSize(col_msk->roi->width, col_msk->roi->height), 8, 3);
					cvCopy(col_msk, msk_han);
					cvResetImageROI(col_msk);

					cvCircle(msk_han, cvPoint((int)(sum_x / num), (int)(sum_y / num)),
						(int)(rad*0.7), CV_RGB(0, 0, 255), 6);
					cvLine(msk_han, sub_cen_pt, fin_pt, CV_RGB(255, 0, 255), 3);
					cvLine(msk_han, cvPoint(0, pt2a.y - pt1.y), cvPoint(pt2a.x - pt1.x, pt2a.y - pt1.y), CV_RGB(255, 255, 0), 2);
					cvCircle(msk_han, sub_cen_pt, 6, CV_RGB(0, 255, 0), -1);
					cvCircle(msk_han, fin_pt, 6, CV_RGB(0, 255, 0), -1);

					IplImage *ringblob = cvCreateImage(cvGetSize(col_han), 8, 1);
					cvZero(ringblob);

					uchar *data_ri = (uchar*)ringblob->imageData;

					if (sub_skin->height < ringblob->height){

						for (int j = 0; j < sub_skin->height; j++)
						for (int i = 0; i < sub_skin->widthStep; i++)
							data_ri[j*ringblob->widthStep + i] = data_sub[j*sub_skin->widthStep + i];
					}

					cvReleaseImage(&col_han);
					cvReleaseImage(&msk_han);
					cvReleaseImage(&ringblob);
				}

				cvReleaseImage(&sub_skin);
				cvReleaseImage(&circle);
				cvReleaseImage(&finger);

				cvClearSeq(ptseq);
				cvReleaseMemStorage(&storage);//�޸� ����
			}//labeling for�� ��

			cvReleaseImage(&skin);
		}//fst_label ; Averaging Background & Labeling








		int bin_w;

		cvSetZero(image);

		cvCopy(frame, image, msk);
		cvCvtColor(image, hsv, CV_BGR2HSV);

		if (track_object){

			int _vmin = vmin, _vmax = vmax;

			cvInRangeS(hsv, cvScalar(0, smin, MIN(_vmin, _vmax), 0),
				cvScalar(180, 256, MAX(_vmin, _vmax), 0), mask);// hsv���� low~upper ���� �� mask�� ����
			cvSplit(hsv, hue, 0, 0, 0);// hue�� �и�?

			//������׷� �׸���
			if (track_object < 0){// ȭ�� �巡���Ͽ� ��ư ��������(= -1)

				float max_val = 0.f;
				cvSetImageROI(hue, selection);// �巡�׷� ������ Rect �κи�.
				cvSetImageROI(mask, selection);
				cvCalcHist(&hue, hist, 0, mask);// ������׷����
				cvGetMinMaxHistValue(hist, 0, &max_val, 0, 0);// Max �� ���
				cvConvertScale(hist->bins, hist->bins, max_val ? 255. / max_val : 0., 0);// hins :1D �迭. max���̸� 255/max ���ؼ� 255�̻� ���� �ʵ���.
				cvResetImageROI(hue);
				cvResetImageROI(mask);
				track_window = selection;
				track_object = 1;// -1���� 1�� �������. ������׷��� ��ư �������� �ѹ� ����.

				cvZero(histimg);
				bin_w = histimg->width / hdims;//hdims ����(=16)��ŭ ������ bin_w �ϳ��� ���̸� ��´�.(bin_w = 320/16 = 20)
				for (int i = 0; i < hdims; i++){
					int val = cvRound(cvGetReal1D(hist->bins, i)*histimg->height / 255);//val : hist�迭�� Max����,MAX * height/255 ���� �ٲ�. (0<MAX<255)
					CvScalar color = hsv2rgb(i*180.f / hdims);
					cvRectangle(histimg, cvPoint(i*bin_w, histimg->height),
						cvPoint((i + 1)*bin_w, histimg->height - val),
						color, -1, 8, 0);// ���� �ݴ��� ������ �̿��� �簢�� �׸���
				}
			}

			// camshift ����
			cvCalcBackProject(&hue, backproject, hist);
			cvAnd(backproject, mask, backproject, 0);//HSV�� mask ����� AND����

			cvCamShift(backproject, track_window,
				cvTermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1),
				&track_comp, &track_box);
			track_window = track_comp.rect;// track_window ������Ʈ. Rect ���� ������Ʈ�Ͽ� Tracking



			//������ ũ�� ����. �޸� ���� ��ħ.
			if (track_window.height > 200)
				track_window.height = 200;
			if (track_window.width > 200)
				track_window.width = 200;

			if (backproject_mode)
				cvCvtColor(backproject, image, CV_GRAY2BGR);
			if (!image->origin)
				track_box.angle = -track_box.angle;//angle �� ������? Ÿ�� �׸��� �ʿ�.

			//       cvEllipseBox(frame, track_box, CV_RGB(255,0,0), 3, CV_AA, 0 );// Ÿ�� �׸���

			//��ü�� �߽ɰ�
			pt.x = (int)(track_box.center.x);
			pt.y = (int)(track_box.center.y);

			//Rect�� ������
			CvPoint pt1;
			pt1 = cvPoint(track_window.x, (int)(track_window.y - 30));// Rect ��ġ�� +y��������� 80��ŭ shift.
			//			pt1 = cvPoint(track_window.x,(int)(track_window.y));// Rect ��ġ�� +y��������� 80��ŭ shift.

			CvPoint n_pt = cvPoint(pt.x - 20, pt.y - 100);//�⺻ (x, y-90)
			int n_box_width = (int)(track_box.size.width*0.7);
			int win_width = (int)(track_window.width*1.2);
			int win_height = (int)(track_window.height);

			//And ���� �ϱ��� mask ���� ����� (track_box�� center�� �̿�)
			cvSetZero(mask_bp);
			cvCircle(mask_bp, n_pt, n_box_width, CV_RGB(255, 255, 255), -1);
			cvDrawRect(mask_bp, pt1, cvPoint(pt1.x + win_width, pt1.y + win_height), CV_RGB(255, 255, 255), -1);
			cvShowImage("BP", mask_bp);// ����ũ ���

			cvAnd(msk, mask_bp, mask_han, 0);


			cvZero(hand);
			cvCopy(image, hand, mask_han);//3ä�� hand ���� ��� ���� ǥ��


			/////////////////////////////////////////////////////////////////////
			//convexhull�� �̿��� ���� ã��
			storage = cvCreateMemStorage(0);
			CvPoint pt0;
			CvSeq* ptseq = cvCreateSeq(CV_SEQ_KIND_GENERIC | CV_32SC2, sizeof(CvContour), sizeof(CvPoint), storage);
			CvSeq* hull;

			for (int j = 0; j < mask_han->height; j++)  //Dst�� ����ó�� �� �̹���
			for (int i = 0; i < mask_han->width; i++)
			{
				if ((unsigned char)mask_han->imageData[j*mask_han->widthStep + i] == 255)
				{
					pt0.x = i;
					pt0.y = j;
					cvSeqPush(ptseq, &pt0);
				}
			}

			pt0.x = 0, pt0.y = 0;// �ܰ��� �׸��� �ʿ��� ��ǥ �ʱ�ȭ

			double fMaxDist = 0.0;
			CvPoint corner;	CvPoint end_pt;
			corner = cvPoint(0, 0);

			if (ptseq->total != 0){//ptseq ���� 0�� ��� cvConvexHull2()�Լ� ��� ���ϹǷ� ������ ����.

				hull = cvConvexHull2(ptseq, 0, CV_COUNTER_CLOCKWISE, 0);

				for (int x = 0; x < hull->total; x++){

					//�ܰ��� �׸���
					CvPoint hull_pt = **CV_GET_SEQ_ELEM(CvPoint*, hull, x);

					if (pt0.x == 0 && pt0.y == 0)//�ʱⰪ ����
					{
						pt0 = hull_pt;	end_pt = pt0;
					}
					cvLine(hand, pt0, hull_pt, CV_RGB(255, 0, 0), 2, 8);
					pt0 = hull_pt;

					//������ ������ �ձ�
					if (x == hull->total - 1)
						cvLine(hand, hull_pt, end_pt, CV_RGB(255, 0, 0), 2, 8);

					//���� ��ǥ ���� �� �ִ�Ÿ� ���ϱ�
					CvPoint cor_pt = **CV_GET_SEQ_ELEM(CvPoint*, hull, x);
					double fDist = sqrt((double)((pt.x - cor_pt.x) * (pt.x - cor_pt.x)
						+ (pt.y - cor_pt.y) * (pt.y - cor_pt.y)));

					if (fDist > fMaxDist)
					{
						corner = cor_pt;
						fMaxDist = fDist;
					}
				}
			}

			////////////////////////////////////////////////////////���� ã�� ��//////////////

			if (corner.x != 0 && corner.y != 0) //convexhull ��� ���ҽ� (0,0)���� �� �ߴ°� �����ϱ� ����.
				cvLine(hand, pt, corner, CV_RGB(255, 255, 0), 1, 8);// �߽����� ������ �մ� �� �׸���.

			///////////////////////////�� ��� ROI ���� & �հ��� ������ �̿��� Ŭ�� �̺�Ʈ- mask ROI ũ�� ����
			CvPoint c_pt, c_pt1, r_pt1;
			c_pt = cvPoint((int)(pt.x - track_box.size.width), (int)(pt.y - track_box.size.width));
			c_pt1 = cvPoint((int)(pt.x + track_box.size.width), (int)(pt.y + track_box.size.width));
			r_pt1 = cvPoint((int)(pt1.x + track_window.width), (int)(pt1.y + track_window.height));
			//	printf("%d %d %d %d \n",pt1.x,pt1.y,c_pt.x,c_pt.y);

			//pt1�� c_pt ��ǥ �̿��Ͽ� ���� ū ROI ���ϱ�(x,y�� �ּ� �ִ� ���� ���� ���ϱ�)
			CvRect r_roi;
			r_roi.x = (int)MIN(pt1.x, c_pt.x);
			r_roi.y = (int)MIN(pt1.y, c_pt.y);
			r_roi.width = (int)(MAX(r_pt1.x, c_pt1.x) - r_roi.x);
			r_roi.height = (int)(MAX(r_pt1.y, c_pt1.y) - r_roi.y);

			//Rect ���� ����
			if (r_roi.x<0)	r_roi.x = 0;
			if (r_roi.y<0)	r_roi.y = 0;
			if (r_roi.width>640)	r_roi.width = 640;
			if (r_roi.height>480) r_roi.height = 480;

			//		printf("%d %d %d %d \n",r_roi.x,r_roi.y,r_roi.width,r_roi.height);

			cvSetImageROI(mask_han, r_roi);
			sub_han = cvCreateImage(cvSize(mask_han->roi->width, mask_han->roi->height), 8, 1);//roi �߿�!(cvCopy��������)
			cvCopy(mask_han, sub_han);
			cvResetImageROI(mask_han);


			circle = cvCreateImage(cvGetSize(sub_han), 8, 1);
			cvSetZero(circle);
			cvCircle(circle, cvPoint(pt.x - r_roi.x, pt.y - r_roi.y), (int)(fMaxDist*0.8), CV_RGB(255, 255, 255), 6);
			cvAnd(sub_han, circle, sub_han, 0);
			//	cvSaveImage("c:/test/finger.jpg",sub_skin);

			cvErode(sub_han, sub_han, 0, 1);
			cvDilate(sub_han, sub_han, 0, 1);
			cvDilate(sub_han, sub_han, 0, 1);

			//cvShowImage("sub_han",sub_han);	

			//�߽��� ǥ��
			cvCircle(hand, pt, 6, CV_RGB(0, 255, 0), 3);// track_box.center �� �̿�
			//track_window ǥ��
			//	cvDrawRect(frame,pt1,cvPoint(pt1.x+track_window.width,pt1.y+track_window.height),CV_RGB(255,255,0),3);// track_comp.rect �� �̿�

			cvShowImage("hand", hand);



			CBlobLabeling fin_n;
			fin_n.SetParam(sub_han, 50);
			fin_n.DoLabeling();

			//point_s �ʱⰪ�� ���ο� �� ���ʷ� ������Ʈ
			if (point_s.x == 0 || point_s.y == 0)//point_s�� 0�̸� ��� �÷����Ǿ� ȭ�� �����ϴܿ� ����Ʈ ��ġ�ϰ� ��.
			{
				point_s.x = corner.x;
				point_s.y = corner.y;
			}

			CvRect r_rec, g_rec, b_rec;
			r_rec.x = 319;	r_rec.y = 0;	r_rec.width = 100;	r_rec.height = 60;
			g_rec.x = 429;	g_rec.y = 0;	g_rec.width = 100;	g_rec.height = 60;
			b_rec.x = 539;	b_rec.y = 0;	b_rec.width = 100;	b_rec.height = 60;

			cvDrawRect(frame, cvPoint(r_rec.x, r_rec.y), cvPoint(r_rec.x + r_rec.width, r_rec.y + r_rec.height), CV_RGB(255, 0, 0), -1, 8);
			cvDrawRect(frame, cvPoint(g_rec.x, g_rec.y), cvPoint(g_rec.x + g_rec.width, g_rec.y + g_rec.height), CV_RGB(255, 255, 0), -1, 8);
			cvDrawRect(frame, cvPoint(b_rec.x, b_rec.y), cvPoint(b_rec.x + b_rec.width, b_rec.y + b_rec.height), CV_RGB(0, 255, 0), -1, 8);

			cvDrawRect(col_msk_n, cvPoint(r_rec.x, r_rec.y), cvPoint(r_rec.x + r_rec.width, r_rec.y + r_rec.height), cvScalarAll(255), -1, 8);
			cvDrawRect(col_msk_n, cvPoint(g_rec.x, g_rec.y), cvPoint(g_rec.x + g_rec.width, g_rec.y + g_rec.height), cvScalarAll(255), -1, 8);
			cvDrawRect(col_msk_n, cvPoint(b_rec.x, b_rec.y), cvPoint(b_rec.x + b_rec.width, b_rec.y + b_rec.height), cvScalarAll(255), -1, 8);

			if (fin_n.m_nBlobs == 1){
				cvLine(draw_msk, cvPoint(corner.x, corner.y), cvPoint(point_s.x, point_s.y), cvScalarAll(255), 5, 8);
				cvLine(draw_col, cvPoint(corner.x, corner.y), cvPoint(point_s.x, point_s.y), d_color, 5, 8);
			}

			point_s.x = corner.x;
			point_s.y = corner.y;

			cvAnd(draw_msk, col_msk_n, col_msk_n);// draw_msk �������� ���� ROI �������� ��ġ�� �κ��� �ִ��� Ȯ��.

			//R,G,B R �簢���� ���� ����
			cvSetImageROI(col_msk_n, r_rec);
			IplImage *r_col = cvCreateImage(cvSize(col_msk_n->roi->width, col_msk_n->roi->height), 8, 1);
			cvCopy(col_msk_n, r_col);
			cvResetImageROI(col_msk_n);

			cvSetImageROI(col_msk_n, g_rec);
			IplImage *g_col = cvCreateImage(cvSize(col_msk_n->roi->width, col_msk_n->roi->height), 8, 1);
			cvCopy(col_msk_n, g_col);
			cvResetImageROI(col_msk_n);

			cvSetImageROI(col_msk_n, b_rec);
			IplImage *b_col = cvCreateImage(cvSize(col_msk_n->roi->width, col_msk_n->roi->height), 8, 1);
			cvCopy(col_msk_n, b_col);
			cvResetImageROI(col_msk_n);

			int col_h = r_col->height;
			int col_w = r_col->widthStep;

			for (int j = 0; j < col_h; j++)
			for (int i = 0; i < col_w; i++)
			{
				if ((uchar)r_col->imageData[j*r_col->widthStep + i] == 255)
				{
					d_color = CV_RGB(255, 0, 0);
					j = col_h;	i = col_w;
				}

				if ((uchar)g_col->imageData[j*g_col->widthStep + i] == 255)
				{
					d_color = CV_RGB(255, 255, 0);
					j = col_h;	i = col_w;
				}

				if ((uchar)b_col->imageData[j*b_col->widthStep + i] == 255)
				{
					d_color = CV_RGB(0, 255, 0);
					j = col_h;	i = col_w;
				}
			}

			// draw_msk ���󿡼� R,G,B �� ROI���� ���� �����͸� �ʱ�ȭ// for�� ��Ȱ�� ���� ��Ű�� ����.
			cvDrawRect(draw_msk, cvPoint(r_rec.x, r_rec.y), cvPoint(b_rec.x + b_rec.width, b_rec.y + b_rec.height), cvScalarAll(0), -1, 8);

			//printf("v0=%f v1=%f v2=%f v3=%f \n",d_color.val[0],d_color.val[1],d_color.val[2],d_color.val[3]);

			//if(d_color.val[0]==255 || d_color.val[1]==255 || d_color.val[2]==255)
			cvCopy(draw_col, frame, draw_msk);


		}//if�� Tracking �κ� 
		cvReleaseMemStorage(&storage);
		cvReleaseImage(&sub_han);
		cvReleaseImage(&circle);

		if (select_object && selection.width > 0 && selection.height > 0)// �巡�� ���� �κ�
		{
			cvSetImageROI(image, selection);
			cvXorS(image, cvScalarAll(255), image, 0);
			cvResetImageROI(image);
		}

		cvShowImage("CamShiftDemo", image);
		cvShowImage("frame", frame);
		//		cvShowImage("result",hand_col);

		//		t = (double)cvGetTickCount() - t;
		//		printf( "detection time = %g ms\n", t/((double)cvGetTickFrequency()*1000.) );		
		//		fprintf(fp,"%g ",t/((double)cvGetTickFrequency()*1000.));

		int c = cvWaitKey(10);
		if ((char)c == 27)
			break;

		// key �Է� �� event
		switch ((char)c)
		{
		case 'b':
			backproject_mode ^= 1;
			break;
		case 'c':
			cvZero(draw_msk);
			break;
		case 'm':
			cvSetZero(msk);
			break;
		case 'h':
			show_hist ^= 1;
			if (!show_hist)
				cvDestroyWindow("Histogram");
			else
				cvNamedWindow("Histogram", 1);
			break;
		default:
			;
		}
	}
	cvReleaseCapture(&capture);
	//	cvReleaseVideoWriter(&videoOut);
	cvReleaseImage(&image);

	cvReleaseImage(&av);
	cvReleaseImage(&sgm);
	cvReleaseImage(&lower);
	cvReleaseImage(&upper);
	cvReleaseImage(&tmp);
	cvReleaseImage(&dst);
	cvReleaseImage(&msk);
	cvReleaseImage(&mask_bp);
	cvReleaseImage(&mask_han);
	cvReleaseImage(&hand_col);
	cvReleaseImage(&col_msk);
	cvReleaseImage(&draw_msk);
	cvReleaseImage(&col_msk_n);

	cvReleaseImage(&r_col);
	cvReleaseImage(&g_col);
	cvReleaseImage(&b_col);

	cvReleaseImage(&hsv);
	cvReleaseImage(&hue);
	cvReleaseImage(&mask);
	cvReleaseImage(&backproject);
	cvReleaseImage(&histimg);

	cvReleaseMemStorage(&storage);

	cvDestroyAllWindows();

	return 0;
}


