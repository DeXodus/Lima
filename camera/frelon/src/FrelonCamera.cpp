#include "FrelonCamera.h"
#include "RegEx.h"
#include <sstream>

using namespace lima::Frelon;
using namespace std;

const double Camera::HorzBinSleepTime = 2;

Camera::Camera(Espia::SerialLine& espia_ser_line)
	: m_ser_line(espia_ser_line)
{

}

SerialLine& Camera::getSerialLine()
{
	return m_ser_line;
}

void Camera::sendCmd(Cmd cmd)
{
	string resp;
	m_ser_line.sendFmtCmd(CmdStrMap[cmd], resp);
}

void Camera::writeRegister(Reg reg, int val)
{
	ostringstream cmd;
	cmd << RegStrMap[reg] << val;
	string resp;
	m_ser_line.sendFmtCmd(cmd.str(), resp);
}

void Camera::readRegister(Reg reg, int& val)
{
	string resp, cmd = RegStrMap[reg] + "?";
	m_ser_line.sendFmtCmd(cmd, resp);
	istringstream is(resp);
	is >> val;
}

void Camera::hardReset()
{
	sendCmd(Reset);
}

void Camera::getVersion(string& ver)
{
	string cmd = RegStrMap[Version] + "?";
	m_ser_line.sendFmtCmd(cmd, ver);
}

void Camera::getComplexSerialNb(int& complex_ser_nb)
{
	readRegister(CompSerNb, complex_ser_nb);
}

void Camera::getSerialNbParam(SerNbParam param, int& val)
{
	int complex_ser_nb;
	getComplexSerialNb(complex_ser_nb);
	val = complex_ser_nb & int(param);
}

void Camera::getCameraType(int& type)
{
	int frelon_2k16;
	getSerialNbParam(F2k16, frelon_2k16);
	type = frelon_2k16 ? 2016 : 2014;
}

void Camera::setChanMode(int chan_mode)
{
	writeRegister(ChanMode, chan_mode);
}

void Camera::getChanMode(int& chan_mode)
{
	readRegister(ChanMode, chan_mode);
}

void Camera::getBaseChanMode(FrameTransferMode ftm, int& base_chan_mode)
{
	base_chan_mode = FTMChanRangeMap[ftm].first;
}

void Camera::getInputChanMode(FrameTransferMode ftm, InputChan input_chan,
			      int& chan_mode)
{
	getBaseChanMode(ftm, chan_mode);
	const InputChanList& chan_list = FTMInputChanListMap[ftm];
	InputChanList::const_iterator it;
	it = find(chan_list.begin(), chan_list.end(), input_chan);
	if (it == chan_list.end())
		throw LIMA_HW_EXC(InvalidValue, "Invalid input channel");
	chan_mode += it - chan_list.begin();
}

void Camera::setInputChan(InputChan input_chan)
{
	FrameTransferMode ftm;
	getFrameTransferMode(ftm);
	int chan_mode;
	getInputChanMode(ftm, input_chan, chan_mode);
	setChanMode(chan_mode);
}

void Camera::getInputChan(InputChan& input_chan)
{
	FrameTransferMode ftm;
	getFrameTransferMode(ftm);
	int chan_mode, base_chan_mode;
	getChanMode(chan_mode);
	getBaseChanMode(ftm, base_chan_mode);
	input_chan = FTMInputChanListMap[ftm][chan_mode - base_chan_mode];
}

void Camera::setFrameTransferMode(FrameTransferMode ftm)
{
	InputChan input_chan;
	getInputChan(input_chan);
	int chan_mode;
	getInputChanMode(ftm, input_chan, chan_mode);
	setChanMode(chan_mode);
}

void Camera::getFrameTransferMode(FrameTransferMode& ftm)
{
	int chan_mode;
	getChanMode(chan_mode);

	FTMChanRangeMapType::const_iterator it, end = FTMChanRangeMap.end();
	for (it = FTMChanRangeMap.begin(); it != end; ++it) {
		ftm = it->first;
		const ChanRange& range = it->second;
		if ((chan_mode >= range.first) && (chan_mode < range.second))
			return;
	}

	throw LIMA_HW_EXC(Error, "Invalid chan mode");
}

void Camera::getFrameDim(FrameDim& frame_dim)
{
	frame_dim = MaxFrameDim;
	FrameTransferMode ftm;
	getFrameTransferMode(ftm);
	if (ftm == FTM)
		frame_dim /= Point(1, 2);
}

void Camera::setFlipMode(int flip_mode)
{
	writeRegister(Flip, flip_mode);
}

void Camera::getFlipMode(int& flip_mode)
{
	readRegister(Flip, flip_mode);
}

void Camera::setFlip(const Point& flip)
{
	int flip_mode = (bool(flip.x) << 1) | (bool(flip.y) << 0);
	setFlipMode(flip_mode);
}

void Camera::getFlip(Point& flip)
{
	int flip_mode;
	getFlipMode(flip_mode);
	flip.x = (flip_mode >> 1) & 1;
	flip.y = (flip_mode >> 0) & 1;
}

void Camera::setBin(const Bin& bin)
{
	Bin curr_bin;
	getBin(curr_bin);
	if (bin == curr_bin)
		return;

	writeRegister(BinHorz, bin.getX());
	Sleep(HorzBinSleepTime);
	writeRegister(BinVert, bin.getY());
}

void Camera::getBin(Bin& bin)
{
	int bin_x, bin_y;
	readRegister(BinHorz, bin_x);
	readRegister(BinVert, bin_y);
	bin = Bin(bin_x, bin_y);
}

void Camera::setRoiMode(RoiMode roi_mode)
{
	bool roi_hw   = (roi_mode == Slow) || (roi_mode == Fast);
	bool roi_fast = (roi_mode == Fast) || (roi_mode == Kinetic);
	bool roi_kin  = (roi_mode == Kinetic);

	writeRegister(RoiEnable,  roi_hw);
	writeRegister(RoiFast,    roi_fast);
	writeRegister(RoiKinetic, roi_kin);
}

void Camera::getRoiMode(RoiMode& roi_mode)
{
	int roi_hw, roi_fast, roi_kin;
	readRegister(RoiEnable,  roi_hw);
	readRegister(RoiFast,    roi_fast);
	readRegister(RoiKinetic, roi_kin);

	if (roi_fast && roi_kin)
		roi_mode = Kinetic;
	else if (roi_fast && roi_hw)
		roi_mode = Fast;
	else if (roi_hw)
		roi_mode = Slow;
	else
		roi_mode = None;
}

void Camera::getMirror(Point& mirror)
{
	mirror.x = isChanActive(Chan12) || isChanActive(Chan34);
	mirror.y = isChanActive(Chan13) || isChanActive(Chan24);
}

void Camera::getNbChan(Point& nb_chan)
{
	getMirror(nb_chan);
	nb_chan += 1;
}

void Camera::getCcdSize(Size& ccd_size)
{
	FrameDim frame_dim;
	getFrameDim(frame_dim);
	ccd_size = frame_dim.getSize();
}

void Camera::getChanSize(Size& chan_size)
{
	getCcdSize(chan_size);
	Point nb_chan;
	getNbChan(nb_chan);
	chan_size /= nb_chan;
}

void Camera::xformChanCoords(const Point& point, Point& chan_point, 
			     Corner& ref_corner)
{
	Size chan_size;
	getChanSize(chan_size);

	bool good_xchan = isChanActive(Chan1) || isChanActive(Chan3);
	bool good_ychan = isChanActive(Chan1) || isChanActive(Chan2);
	Point flip;
	getFlip(flip);
	XBorder ref_xb = (bool(flip.x) == !good_xchan) ? Left : Right;
	YBorder ref_yb = (bool(flip.y) == !good_ychan) ? Top  : Bottom;

	Point mirror;
	getMirror(mirror);
	if (mirror.x)
		ref_xb = (point.x < chan_size.getWidth())  ? Left : Right;
	if (mirror.y)
		ref_yb = (point.y < chan_size.getHeight()) ? Top  : Bottom;

	ref_corner.set(ref_xb, ref_yb);

	Size ccd_size;
	getCcdSize(ccd_size);

	chan_point = ccd_size.getCornerCoords(point, ref_corner);
}

void Camera::getImageRoi(const Roi& chan_roi, Roi& image_roi)
{
	Point img_tl, chan_tl = chan_roi.getTopLeft();
	Point img_br, chan_br = chan_roi.getBottomRight();
	Corner c_tl, c_br;
	xformChanCoords(chan_tl, img_tl, c_tl);
	xformChanCoords(chan_br, img_br, c_br);

	Roi unbinned_roi(img_tl, img_br);
	Bin bin;
	getBin(bin);
	image_roi = unbinned_roi.getBinned(bin);
}

void Camera::getFinalRoi(const Roi& image_roi, const Point& roi_offset,
			 Roi& final_roi)
{
	Point tl = image_roi.getTopLeft() + roi_offset;
	Point nb_chan;
	getNbChan(nb_chan);
	Size size = image_roi.getSize() * nb_chan;
	final_roi = Roi(tl, size);
}

void Camera::getChanRoi(const Roi& image_roi, Roi& chan_roi)
{
	Bin bin;
	getBin(bin);
	Roi unbinned_roi = image_roi.getUnbinned(bin);
	Point chan_tl, img_tl = unbinned_roi.getTopLeft();
	Point chan_br, img_br = unbinned_roi.getBottomRight();
	Corner c_tl, c_br;
	xformChanCoords(img_tl, chan_tl, c_tl);
	xformChanCoords(img_br, chan_br, c_br);

	chan_roi.setCorners(chan_tl, chan_br);
	chan_tl = chan_roi.getTopLeft();
	chan_br = chan_roi.getBottomRight();

	bool two_xchan = (c_tl.getX() != c_br.getX());
	bool two_ychan = (c_tl.getY() != c_br.getY());
	
	Size chan_size;
	getChanSize(chan_size);
	if (two_xchan)
		chan_br.x = chan_size.getWidth() - 1;
	if (two_ychan)
		chan_br.y = chan_size.getHeight() - 1;

	chan_roi.setCorners(chan_tl, chan_br);
}

void Camera::getImageRoiOffset(const Roi& req_roi, const Roi& image_roi,
			       Point& roi_offset)
{
	Point virt_tl = image_roi.getTopLeft();

	Size ccd_size, image_size;
	getCcdSize(ccd_size);
	image_size = image_roi.getSize();
	Point image_br = image_roi.getBottomRight() + 1;

	Point mirror_tl = ccd_size - image_br - image_size;
	Point req_tl = req_roi.getTopLeft();
	if (req_tl.x >= image_br.x)
		virt_tl.x = mirror_tl.x;
	if (req_tl.y >= image_br.y)
		virt_tl.y = mirror_tl.y;

	roi_offset = virt_tl - image_roi.getTopLeft();
}

void Camera::checkRoiMode(const Roi& roi)
{
	RoiMode roi_mode;
	getRoiMode(roi_mode);
	if (!roi.isActive())
		roi_mode = None;
	else if (roi_mode == None)
		roi_mode = Slow;
	setRoiMode(roi_mode);
}

void Camera::checkRoi(Roi& roi)
{
	if (!roi.isActive())
		return;

	Roi chan_roi;
	Point roi_offset;
	processRoiReq(roi, chan_roi, roi_offset);
}

void Camera::processRoiReq(Roi& roi, Roi& chan_roi, Point& roi_offset)
{
	roi.alignCornersTo(Point(32, 1), Ceil);
	getChanRoi(roi, chan_roi);
	Roi image_roi;
	getImageRoi(chan_roi, image_roi);
	getImageRoiOffset(roi, image_roi, roi_offset);
	getFinalRoi(image_roi, roi_offset, roi);
}

void Camera::setRoi(const Roi& roi)
{
	checkRoiMode(roi);
	if (!roi.isActive())
		return;

	Roi chan_roi, proc_roi = roi;
	Point roi_offset;
	processRoiReq(proc_roi, chan_roi, roi_offset);
	if (proc_roi != roi)
		throw LIMA_HW_EXC(InvalidValue, "Requested roi not valid");

	Point tl  = chan_roi.getTopLeft();
	Size size = chan_roi.getSize();

	writeRegister(RoiPixelBegin, tl.x);
	writeRegister(RoiPixelWidth, size.getWidth());
	writeRegister(RoiLineBegin,  tl.y);
	writeRegister(RoiLineWidth,  size.getHeight());

	m_roi_offset = roi_offset;
}

void Camera::getRoi(Roi& roi)
{
	int rpb, rpw, rlb, rlw;
	readRegister(RoiPixelBegin, rpb);
	readRegister(RoiPixelWidth, rpw);
	readRegister(RoiLineBegin,  rlb);
	readRegister(RoiLineWidth,  rlw);

	Roi chan_roi(Point(rpb, rlb), Size(rpw, rlw));
	Roi image_roi;
	getImageRoi(chan_roi, image_roi);
	getFinalRoi(image_roi, m_roi_offset, roi);
}
