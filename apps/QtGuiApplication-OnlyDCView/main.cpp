#include <QtWidgets/QApplication>

#include "QGridLayout"

#include "DCView/Viewer.h"

//! DCGa
#include "DCGa/TrackballManipulator.h"
#include "DCGa/FlyManipulator.h"

int main(int argc, char *argv[])
{
	//��ʼ��freeglut
	glutInit(&argc, argv);

	QApplication a(argc, argv);

	//! ���dcviewer
	//! ��ȡ��������
	//QString fileName = "D:\\data\\TestData\\LOUTI-5.txt";
	QString fileName = "D:\\data\\selfpdb\\CangSu_JiZai.pdb";
	//QString fileName = "D:\\data\\bieshu-pdb\\bieshu.pdb";


	MPViewer::Viewer* pNewViewer = new MPViewer::Viewer(fileName);
	//MPViewer::Viewer* pNewViewer = new MPViewer::Viewer();

	//! ��װ������
	pNewViewer->AddManipulatorHandle(new DCGa::TrackballManipulator("TrackBall"));
	pNewViewer->AddManipulatorHandle(new DCGa::FlyManipulator("Fly"));
	pNewViewer->showMaximized();


	return a.exec();
}
