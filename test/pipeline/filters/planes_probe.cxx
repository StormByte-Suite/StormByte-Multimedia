#include "../helpers.hxx"

#include <StormByte/logger/threaded_log.hxx>
#include <StormByte/multimedia/pipeline/filters/video/bwdif.hxx>
#include <StormByte/multimedia/pipeline/filters/video/deband.hxx>
#include <StormByte/multimedia/pipeline/filters/video/hqdn3d.hxx>
#include <StormByte/multimedia/pipeline/filters/video/nlmeans.hxx>
#include <StormByte/multimedia/pipeline/filters/video/yadif.hxx>
#include <StormByte/multimedia/pipeline/transcoder.hxx>
#include <StormByte/multimedia/registry.hxx>

#include <cstdio>
#include <iostream>
#include <string>

using namespace StormByte::Multimedia;
using namespace StormByte::Multimedia::Pipeline;
using FFrame = StormByte::Multimedia::FFmpeg::AVFrame;

static FILE* g_dump = nullptr;
static int g_canon = 0;

static int g_feed = -1;
class Feed final: public Filter::Process {
	public:
		Feed(): Filter::Process({}, "feed") {}
		StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Video; }
		void Clean() noexcept override {}
		void Setup() noexcept override {}
		void Process(const Frame&) noexcept override {
			const auto& f = AVFrame();
			FFrame out;
			out.Format(g_feed);
			if (!f.ScaleTo(out, f.Width(), f.Height(), FFrame::Resample::Point, FFrame::Scaler::Sws) || !out.CopyProps(f)) {
				std::fprintf(stderr, "feed failed\n");
				return;
			}
			out.Format(g_feed);
			Save(std::move(out));
		}
};

class Dump final: public Filter::Process {
	public:
		Dump(): Filter::Process({}, "dump") {}
		StormByte::Multimedia::Type Media() const noexcept override { return StormByte::Multimedia::Type::Video; }
		void Clean() noexcept override {}
		void Setup() noexcept override {}
		void Process(const Frame&) noexcept override {
			const auto& f = AVFrame();
			static bool once = false;
			if (!once) { once = true; std::fprintf(stderr, "FILTERED-FMT %s\n", f.FormatName()); }
			FFrame canon;
			canon.Format(g_canon);
			if (!f.ScaleTo(canon, f.Width(), f.Height(), FFrame::Resample::Point, FFrame::Scaler::Sws)) {
				std::fprintf(stderr, "convert failed\n");
				return;
			}
			const int bytes = canon.BitsPerComponent() > 8 ? 2 : 1;
			for (int p = 0; p < 3; ++p)
				for (int r = 0; r < canon.PlaneHeight(p); ++r)
					std::fwrite(canon.Data(p) + r * canon.Linesize(p), 1,
						static_cast<std::size_t>(canon.PlaneWidth(p)) * bytes, g_dump);
			FFrame out = f.Clone();
			Save(std::move(out));
		}
};

int main(int argc, char** argv) {
	if (argc < 6) return 2;
#ifdef _MSC_VER
	::fopen_s(&g_dump, argv[3], "wb");
#else
	g_dump = std::fopen(argv[3], "wb");
#endif
	if (!g_dump) {
		std::perror(argv[3]);
		return 1;
	}
	g_canon = std::stoi(argv[4]);
	const std::string name = argv[2];
	auto codec = Registry::Instance().FindCodec("VP9");
	auto logger = StormByte::Safe::MakeShared<StormByte::Logger::ThreadedLog>(std::cerr, StormByte::Logger::Level::Error, "[%L]");
	Transcoder job{TestLocation(argv[1]), TestLocation("/tmp/pcprobe.mkv"), logger, 2000000000LL};
	auto track = job.Video(0);
	track.Codec(codec->get()).Implementation(ImplementationSide::Encoder, StormByte::Safe::String{"libvpx-vp9"}).BitRate(1000000);
	StormByte::Safe::Shared<StormByte::Logger::Log> none;
	g_feed = std::stoi(argv[5]);
	track.Filter<Feed>();
	if (name == "hqdn3d") track.Filter<Filter::Video::Hqdn3d>(none);
	else if (name == "nlmeans") track.Filter<Filter::Video::NlMeans>(none);
	else if (name == "deband") track.Filter<Filter::Video::Deband>(none);
	else if (name == "yadif") track.Filter<Filter::Video::Yadif>(none, false);
	else if (name == "bwdif") track.Filter<Filter::Video::Bwdif>(none, false);
	track.Filter<Dump>();
	job.Run();
	const int rc = WaitForTranscoder(job);
	std::fclose(g_dump);
	return rc;
}
