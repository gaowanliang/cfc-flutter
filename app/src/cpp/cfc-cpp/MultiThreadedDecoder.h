#pragma once

#include "compression/zstd_decompressor.h"
#include "encoder/Decoder.h"
#include "extractor/Anchor.h"
#include "extractor/Deskewer.h"
#include "extractor/Extractor.h"
#include "extractor/Scanner.h"
#include "fountain/concurrent_fountain_decoder_sink.h"

#include "concurrent/thread_pool.h"
#include <opencv2/opencv.hpp>
#include <fstream>

class MultiThreadedDecoder
{
public:
	MultiThreadedDecoder(std::string data_path, bool legacy_mode);

	inline static clock_t bytes = 0;
	inline static clock_t perfect = 0;
	inline static clock_t decoded = 0;
	inline static clock_t decodeTicks = 0;
	inline static clock_t scanned = 0;
	inline static clock_t scanTicks = 0;
	inline static clock_t extractTicks = 0;

	bool add(cv::Mat mat);

	void stop();

	bool legacy_mode() const;
	unsigned num_threads() const;
	unsigned backlog() const;
	unsigned files_in_flight() const;
	unsigned files_decoded() const;
	std::vector<std::string> get_done() const;
	std::vector<double> get_progress() const;

protected:
	int do_extract(const cv::Mat& mat, cv::Mat& img);
	void save(const cv::Mat& img);

protected:
	bool _legacyMode;
	Decoder _dec;
	unsigned _numThreads;
	turbo::thread_pool _pool;
	concurrent_fountain_decoder_sink<cimbar::zstd_decompressor<std::ofstream>> _writer;
	std::string _dataPath;
};

inline MultiThreadedDecoder::MultiThreadedDecoder(std::string data_path, bool legacy_mode)
	: _legacyMode(legacy_mode)
	, _dec(cimbar::Config::ecc_bytes(), cimbar::Config::color_bits(), legacy_mode? 0 : 1, legacy_mode)
	, _numThreads(std::max<int>(((int)std::thread::hardware_concurrency()/2), 1))
	, _pool(_numThreads, 1)
	, _writer(data_path, cimbar::Config::fountain_chunk_size(cimbar::Config::ecc_bytes(), cimbar::Config::symbol_bits() + cimbar::Config::color_bits(), legacy_mode))
	, _dataPath(data_path)
{
	FountainInit::init();
	_pool.start();
}

inline int MultiThreadedDecoder::do_extract(const cv::Mat& mat, cv::Mat& img)
{
	clock_t begin = clock();

	Scanner scanner(mat);
	std::vector<Anchor> anchors = scanner.scan();
	++scanned;
	scanTicks += (clock() - begin);

	//if (anchors.size() >= 3) save(mat);

	if (anchors.size() < 4)
		return Extractor::FAILURE;

	begin = clock();
	Corners corners(anchors);
	Deskewer de;
	img = de.deskew(mat, corners);
	extractTicks += (clock() - begin);

	return Extractor::SUCCESS;
}

inline bool MultiThreadedDecoder::add(cv::Mat mat)
{
	return _pool.try_execute( [&, mat] () {
		cv::Mat img;
		int res = do_extract(mat, img);
		if (res == Extractor::FAILURE)
			return;

		// if extracted image is small, we'll need to run some filters on it
		clock_t begin = clock();
		bool should_preprocess = (res == Extractor::NEEDS_SHARPEN);
		int color_correction = _legacyMode? 1 : 2;
		unsigned decodeRes = _dec.decode_fountain(img, _writer, should_preprocess, color_correction);
		bytes += decodeRes;
		++decoded;
		decodeTicks += clock() - begin;

		if (decodeRes >= 6900)
			++perfect;
	} );
}

inline void MultiThreadedDecoder::save(const cv::Mat& mat)
{
	std::stringstream fname;
	fname << _dataPath << "/scan" << (scanned-1) << ".png";
	cv::Mat bgr;
	cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
	cv::imwrite(fname.str(), bgr);
}

inline void MultiThreadedDecoder::stop()
{
	_pool.stop();
}

inline bool MultiThreadedDecoder::legacy_mode() const
{
	return _legacyMode;
}

inline unsigned MultiThreadedDecoder::num_threads() const
{
	return _numThreads;
}

inline unsigned MultiThreadedDecoder::backlog() const
{
	return _pool.queued();
}

inline unsigned MultiThreadedDecoder::files_in_flight() const
{
	return _writer.num_streams();
}

inline unsigned MultiThreadedDecoder::files_decoded() const
{
	return _writer.num_done();
}

inline std::vector<std::string> MultiThreadedDecoder::get_done() const
{
	return _writer.get_done();
}

inline std::vector<double> MultiThreadedDecoder::get_progress() const
{
	return _writer.get_progress();
}
