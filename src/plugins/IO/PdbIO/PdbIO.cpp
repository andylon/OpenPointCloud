#include "PdbIO.h"

//C++
#include <array>

//Qt��׼��
#include "QFile"
#include "QTime"
#include "QFileInfo"
#include "QTextStream"
#include "QtWidgets/QApplication"
#include "QDebug"
#include "QtWidgets/QApplication"

//DCCore
#include "DCCore/Logger.h"
#include "DCCore/CoordinatesShiftManager.h"

//DcGp
#include "DCGp/Custom/GpScalarField.h"
#include "DCGp/Custom/PageLodEntity.h"

#include "PagFileinfo.h"


using namespace DcIo;





DCCore::IoPlugin* PdbIO::s_instance = 0;
QStringList PdbIO::SUPPORT_EXTENSION = QStringList() << "pdb";

DCCore::IoPlugin* LoadPlugin(const char* extension)
{
	return PdbIO::Instance(extension);
}

void UnloadPlugin()
{
	//PdbIO::UnloadInstance();
}

DCCore::IoPlugin* PdbIO::Instance(const char* extension)
{
	bool status = false;
	for (auto i = 0; i < SUPPORT_EXTENSION.size(); ++i)
	{
		if (QString(extension).toUpper() == SUPPORT_EXTENSION.at(i).toUpper())
		{
			status = true;
		}
	}

	if (status)
	{
		if (!s_instance)
		{
			s_instance = new PdbIO();
		}
		return s_instance;
	}
	else
	{
		return nullptr;
	}
}

void PdbIO::UnloadInstance()
{
	if (s_instance)
	{
		delete s_instance;
		s_instance = 0;
	}
}

PdbIO::PdbIO()
	: DCCore::IoPlugin()
{
	
}

PdbIO::~PdbIO()
{
	
}

QString PdbIO::GetName()
{
	return QString("PDBIO");
}

void PdbIO::SetBoxInfo(DcGp::PageLodEntity* pointCloud, QString qFilename, bool isRootFile/* = false*/) const
{
	if (!pointCloud)
	{
		return;
	}

	//1.�ж��ļ�ͷ�Ƿ���ڶ�Ӧ��boxfile
	QFileInfo boxFileinfo(qFilename);
	QString absultPath = boxFileinfo.absolutePath();
	QString basename = boxFileinfo.baseName();

	//����isRootFileȷ�������ļ���
	QString assioFilename;
	if (isRootFile)
	{
		//changshu_L0_X0_Y0.txt
		assioFilename = QString("%1\\%2_L0_X0_Y0.pdb").arg(basename).arg(basename);
	}
	else
	{
		assioFilename = basename + ".pdb";
	}

	QString boxFilename = QString("%1\\%2_box.pdb").arg(absultPath).arg(basename);
	QFile boxFile(boxFilename);
	if (!boxFile.exists())
	{
		return;
	}

	//���������ļ�����box�ļ�������������������pagelod�ڵ�
	QString dataPath = absultPath + "\\";
	pointCloud->SetDatabasePath(dataPath);

	//��boxfile��ȡ��box�߽������ֵ--ָ��pagelod����
	if (!boxFile.open(QIODevice::ReadOnly))
	{
		return;
	}
	char boxLine[500];
	boxFile.readLine(boxLine, 500);
	QStringList boxLineList = QString(boxLine).split(QRegExp(",|\\s+"), QString::SkipEmptyParts);
	Q_ASSERT(boxLineList.size() == 6 || boxLineList.size() == 9);
	DCVector3D minCorner(boxLineList[0].toDouble(), boxLineList[1].toDouble(), boxLineList[2].toDouble());
	DCVector3D maxCorner(boxLineList[3].toDouble(), boxLineList[4].toDouble(), boxLineList[5].toDouble());

	//����range
	float rangeValue = sqrt(0.25*((maxCorner - minCorner).LengthSquared())) * 7;

	//����center
	DCVector3D center = (maxCorner + minCorner)*0.5;
	pointCloud->SetCenter(center + DCVector3D(pointCloud->GetShift().x, pointCloud->GetShift().y, pointCloud->GetShift().z));


	//����rangelist
	DcGp::Range range(0.0f, rangeValue);
	pointCloud->SetRange(range);

	//����rangFilename
	pointCloud->SetFilename(0, assioFilename);
}

//�����ļ�
DcGp::DcGpEntity* PdbIO::LoadFile(const QString& filename, 
	                                            DCCore::CallBackPos* cb, 
												bool alwaysDisplayLoadDialog /* = true */, 
												bool* coordinatesShiftEnabled /* = 0 */, 
												double* coordinatesShift /* = 0 */ ,
												QString kind /*= ""*/)
{
	

	QFile file(filename);

	//�ļ�������
	if (!file.exists())
	{
		DCCore::Logger::Error(QObject::tr("File [%1] doesn\'t exist.").arg(filename));
		return nullptr;
	}

	unsigned long fileSize = file.size();
	//�ļ�Ϊ��
	if (fileSize == 0)
	{
		DCCore::Logger::Message(QObject::tr("File [%1] is empty.").arg(filename));
	}


	//! ������ʼ��
	DataColumnType columnTypes;
	QChar separator;
	unsigned numberOfLines;
	unsigned maxCloudSize;
	int sampleRate;
	unsigned skippedLines;

	//! Ĭ�ϵ����߳��м��ص�ʱ��ֻ֧��Ĭ�ϲ������
	{
		columnTypes.xCoordIndex = 0;
		columnTypes.yCoordIndex = 1;
		columnTypes.zCoordIndex = 2;

		separator = QChar(',');
		numberOfLines = 10;
		maxCloudSize = 0;
		sampleRate = 1;
		skippedLines = 0;
	}

	//! �����ļ���Ϣ�ж�
	//�����ļ�����ȡ�ļ����
	FileInfo fLabel(filename);


	//2.�����ļ���Ŵ���ʲô���ͽڵ㣨����pagelod���� Group��
	//һ��pagelod(�ļ����в���_�ָ�)
	if (!fLabel.isSubfile)
	{
		DcGp::PageLodEntity* pagEntity = LoadCloudFromFormatedAsciiFile(filename,
											columnTypes,
											separator,
											numberOfLines,
											fileSize,
											maxCloudSize,
											cb,
											sampleRate,
											skippedLines,
											alwaysDisplayLoadDialog,
											coordinatesShiftEnabled,
											coordinatesShift);

		if (pagEntity)
		{
			SetBoxInfo(pagEntity, filename, true);

			return pagEntity;
		}
		
		return nullptr;
	}
	else
	{
		//��Ҫ�Ǵ�fLabel��ȡ����һ���������ļ������ܰ˸�Ҳ�����ĸ�
		QStringList ftList = fLabel.subFileList;
		/*fList << "F:\\testbig\\process\\changshu_pfa\\changshu_pagefile\\changshu_L1_X0_Y0.pfa"
		<< "F:\\testbig\\process\\changshu_pfa\\changshu_pagefile\\changshu_L1_X1_Y0.pfa"
		<< "F:\\testbig\\process\\changshu_pfa\\changshu_pagefile\\changshu_L1_X0_Y1.pfa"
		<< "F:\\testbig\\process\\changshu_pfa\\changshu_pagefile\\changshu_L1_X1_Y1.pfa";*/
		
		//!����ʵ����
		DcGp::DcGpEntity* group = new DcGp::DcGpEntity("group");
		for (auto i = 0; i != ftList.size(); ++i)
		{
			//�����ļ�������һ��group�ڵ㣬�����ĸ��ӽڵ�
			//�����ļ���������pagelod�ڵ�
			try
			{

				DcGp::PageLodEntity* pagEntity = LoadCloudFromFormatedAsciiFile(ftList[i],
																		columnTypes,
																		separator,
																		numberOfLines,
																		fileSize,
																		maxCloudSize,
																		cb,
																		sampleRate,
																		skippedLines,
																		alwaysDisplayLoadDialog,
																		coordinatesShiftEnabled,
																		coordinatesShift);
				if (pagEntity)
				{
					SetBoxInfo(pagEntity, ftList[i], false);

					group->AddChild(pagEntity);
				}
				

			}
			catch (...)
			{
				auto aa = 5;
			}

		}

		if (group->GetChildrenNumber())
		{
			return group;
		}
		else
		{
			delete group;
			group = nullptr;
			return nullptr;
		}
		
	}
	
	
}

//���ļ��м�������
DcGp::PageLodEntity* PdbIO::LoadCloudFromFormatedAsciiFile(const QString& fileName,
																		const DataColumnType columnType,
																		QChar separator,
																		unsigned numberOfLines,
																		qint64 fileSize,
																		unsigned maxCloudSize,
																		DCCore::CallBackPos* cb, 
																		int sampleRate,
																		unsigned skippedLines,
																		bool alwaysDisplayLoadDialog,
																		bool* coordinatesShiftEnabled,
																		double* coordinatesShift)
{
	QFile file(fileName);

	//�ļ�������
	if (!file.exists())
	{
		DCCore::Logger::Error(QObject::tr("File [%1] doesn\'t exist.").arg(fileName));
		return false;
	}

	fileSize = file.size();
	//�ļ�Ϊ��
	if (fileSize == 0)
	{
		DCCore::Logger::Message(QObject::tr("File [%1] is empty.").arg(fileName));
	}

	//���ļ�;
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	//ͨ���������Թ����Ե�����;
	long count = 0;
	char currentLine[500];
	//����������ݵ������Ŀ
	long maxSize = (numberOfLines - skippedLines);

	//��ȡ������
	int samplingRate = 1;
	if (alwaysDisplayLoadDialog)
	{
		samplingRate = sampleRate; //DlgSamplingRate::Rate();
		if (samplingRate < 0)
		{
			return false;
		}
	}

	//����һ�����ƶ���,�������������Ŀ
	DcGp::PageLodEntity* pointCloud = new DcGp::PageLodEntity(QFileInfo(fileName).baseName());

	PerLineData record;
	Point_3D point(0,0,0);
	Point_3D shift(0,0,0);
	//���Ĭ����ɫ
	PointColor color = {255, 255, 255};


	//�Ƿ�ʶ���ѧ������
	bool scientificNotation = true;

	const char* loadingStr = "Loading";

	while(file.readLine(currentLine, 500) > 0)
	{
		if (++count <= skippedLines)
		{
			continue;
		}

		//������ڿ��������
		if ('\n' == currentLine[0])
		{
			continue;
		}

		//��ȡ�ļ���ÿ������;
		QStringList line = QString(currentLine).split(separator, QString::SkipEmptyParts);

		//��������е����ݲ�������ѡ�����ͣ������
		if (line.size() < columnType.ValidTypeNumber())
		{
			continue;
		}

		//��ȡXYZ����
		if (columnType.xCoordIndex >= 0)
		{
			point[0] = line[columnType.xCoordIndex].toDouble(&scientificNotation);
		} 
		if (columnType.yCoordIndex >= 0)
		{
			point[1] = line[columnType.yCoordIndex].toDouble(&scientificNotation);
		} 
		if (columnType.zCoordIndex >= 0)
		{
			point[2] = line[columnType.zCoordIndex].toDouble(&scientificNotation);
		} 
		//����Ƕ����һ�����ݣ��ж��Ƿ���Ҫ
		if (pointCloud->Size() == 0 && !coordinatesShiftEnabled)
		{
			if (DCCore::CoordinatesShiftManager::Handle(point, shift))
			{
				pointCloud->SetShift(shift);
			}
		}
		else if (pointCloud->Size() == 0 && coordinatesShiftEnabled)
		{
			shift[0] = coordinatesShift[0];
			shift[1] = coordinatesShift[1];
			shift[2] = coordinatesShift[2];
			pointCloud->SetShift(shift);
		}
		//;
		//�����ݶ�ȡ����;
		pointCloud->AddPoint(DCVector3D(point[0] + shift[0], point[1] + shift[1], point[2] + shift[2]));

		if (columnType.HasRGBColors())
		{
			if (columnType.rgbRedIndex >= 0)
			{
				color[0] = line[columnType.rgbRedIndex].toFloat();
			}  
			if (columnType.rgbGreenIndex >= 0)
			{
				color[1] = line[columnType.rgbGreenIndex].toFloat();
			}  
			if (columnType.rgbBlueIndex >= 0)
			{
				color[2] = line[columnType.rgbBlueIndex].toFloat();
			}  
			if (columnType.grayIndex >= 0)
			{
				record.gray = line[columnType.grayIndex].toFloat();
			}  
			pointCloud->AddColor(color);
		}
		
	}

	file.close();
	

	return pointCloud;
}

bool PdbIO::SaveToFile(DcGp::DcGpEntity* entity, QString filename)
{
	//д������
	QFile file(filename);
	if (!file.open(QFile::WriteOnly | QFile::Truncate))
	{
		return false;
	}
	QTextStream stream(&file);
	//���ø�ʽ����
	stream.setRealNumberNotation(QTextStream::FixedNotation);
	stream.setRealNumberPrecision(3);

	DcGp::DcGpPointCloud* pointCloud = 0;
	if (entity && entity->IsA("DcGpPointCloud"))
	{
		pointCloud = static_cast<DcGp::DcGpPointCloud*>(entity);
	}
	else
	{
		return false;
	}


	//��ȡ��������ɫ����������Ϣ
	unsigned long number = pointCloud->Size();
	bool writeColors = pointCloud->HasColors();
	bool writeNormals = pointCloud->HasNormals();
	bool writeField = false;
	if (pointCloud->GetCurrentScalarField())
	{
		writeField = true;
	}
	

	//����ƫ����
	Point_3D shift = pointCloud->GetShift();

	//д������
	for (unsigned long i = 0; i != number; ++i)
	{
		//����һ����ǰ�б���
		QString line;

		//д�뵱ǰ����
		Point_3D point = Point_3D::FromArray(pointCloud->GetPoint(i).u);
		line.append(QString("%1").arg(point.x - shift.x, 0, 'f', 3));
		line.append(" "); //��ӷָ���
		line.append(QString("%1").arg(point.y - shift.y, 0, 'f', 3));
		line.append(" "); //��ӷָ���
		line.append(QString("%1").arg(point.z - shift.z, 0, 'f', 3));
		line.append(" "); //��ӷָ���

		//����һ����ǰ�б����洢��ɫֵ
		QString color;
		if (writeColors)
		{
			//���rgb��ɫ
			PointColor rgbColor = pointCloud->GetPointColor(i);
			color.append(" ");
			color.append(QString::number(rgbColor[0]));
			color.append(" ");
			color.append(QString::number(rgbColor[1]));
			color.append(" ");
			color.append(QString::number(rgbColor[2]));

			line.append(color);
		}

		//����һ����ǰ�б����洢����ֵ
		QString normal;
		if (writeNormals)
		{
			//���normal
			DCVector3D pNormal = pointCloud->GetPointNormal(i);
			normal.append(" ");
			normal.append(QString::number(pNormal[0]));
			normal.append(" ");
			normal.append(QString::number(pNormal[1]));
			normal.append(" ");
			normal.append(QString::number(pNormal[2]));

			line.append(normal);
		}

		//����һ����ǰ�д洢field
		QString field;
		if (writeField)
		{
			//��ӵ�ǰfieldֵ
			//��ȡ��ǰ���Ӧ�ı�����ֵ
			double value = pointCloud->GetCurrentScalarField()->GetPointScalarValue(i);
			field.append(" ");
			field.append(QString::number(value));

			line.append(field);
		}

		stream << line << "\n";
	}

	file.close();
	return true;
}