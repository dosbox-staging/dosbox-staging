
#include <cassert>
#include <stdio.h>

class SDL_Point {
public:
	int x            = 0;
	int y            = 0;
	const char* name = nullptr;
	constexpr SDL_Point(const int _x, const int _y, const char* _name = nullptr)
	        : x(_x),
	          y(_y),
	          name(_name){};
};

constexpr SDL_Point eight_by_five_ratio = {8, 5, "widescreen"};
constexpr SDL_Point four_by_three_ratio = {4, 3, "standard"};

constexpr SDL_Point list_of_aspect_ratios[] = {eight_by_five_ratio,
                                               four_by_three_ratio};

constexpr int small_percent  = 33;
constexpr int medium_percent = 50;
constexpr int large_percent  = 75;
constexpr int full_percent   = 100;

constexpr int list_of_percent_scalers[] = {small_percent,
                                           medium_percent,
                                           large_percent,
                                           full_percent};

// https://en.wikipedia.org/wiki/Display_resolution
constexpr SDL_Point list_of_screen_dimensions[] = {
        {640, 480, "VGA"},
        {720, 480, "480p NTSC"},
        {726, 576, "480p PAL"},
        {800, 600, "SVGA"},
        {1024, 768, "XGA"},
        {1280, 720, "720p"},
        {1280, 800, "WXGA"},
        {1280, 1024, "Super-eXtended Graphics Array (SXGA)"},
        {1360, 768, "High Definition (HD)"},
        {1366, 768, "High Definition (HD)"},
        {1440, 900, "WXGA+"},
        {1536, 864, "N/A"},
        {1600, 900, "High Definition Plus (HD+)"},
        {1680, 1050, "WSXGA+"},
        {1600, 1200, "High Definition Plus (HD+)"},
        {1920, 1080, "Full High Definition (FHD)"},
        {1920, 1200, "Wide Ultra Extended Graphics Array (WUXGA)"},
        {2048, 872, "Cinemascope"},
        {2048, 1152, "QWXGA 16:9"},
        {2048, 1536, "QXGA 4:3"},
        {2048, 1556, "Film (full-aperture)"},
        {2560, 1080, "UWFHD roughly 21:9"},
        {2560, 1440, "Quad High Definition (QHD)"},
        {2560, 1600, "WQXGA 16:10"},
        {3440, 1440, "Wide Quad High Definition (WQHD)"},
        {3840, 2160, "4K or Ultra High Definition (UHD)"},
        {4096, 3072, "4K reference resolution"},
        {7680, 4320, "8K"},
};

SDL_Point calc_nearest_aspect_corrected_dimensions(const SDL_Point& source_dimensions,
                                                   const SDL_Point& aspect_ratios,
                                                   const int in_multiples_of = 20)
{
	auto to_nearest_multiple = [](const int source_dimension,
	                              const int aspect_ratio,
	                              const int in_multiples_of) {
		assert(aspect_ratio > 0);
		assert(in_multiples_of > 0);
		const auto aspect_multiple = aspect_ratio * in_multiples_of;
		assert(source_dimension > aspect_multiple);
		return (source_dimension / aspect_multiple) * aspect_multiple;
	};

	auto width = to_nearest_multiple(source_dimensions.x,
	                                 aspect_ratios.x,
	                                 in_multiples_of);

	auto height = to_nearest_multiple(source_dimensions.y,
	                                  aspect_ratios.y,
	                                  in_multiples_of);

	const auto width_with_ratio  = width * aspect_ratios.y;
	const auto height_with_ratio = height * aspect_ratios.x;

	// screen is landscape, so bound the width using the height
	if (width_with_ratio > height_with_ratio) {
		width = height_with_ratio / aspect_ratios.y;
	}
	// screen is portrait, so bound the height using the width
	else if (height_with_ratio > width_with_ratio) {
		height = width_with_ratio / aspect_ratios.x;
	}

	return {width, height};
}

int main()
{
	auto print_dimensions = [](const SDL_Point& dimensions) {
		printf("%4dx%4d ", dimensions.x, dimensions.y);
	};

	for (const auto& aspect_ratio : list_of_aspect_ratios) {
		printf("Sizes for %s aspect ratio %d:%d\n",
		       aspect_ratio.name,
		       aspect_ratio.x,
		       aspect_ratio.y);
		printf("   Screen     Small    Medium     Large      Full  Description\n");

		for (const auto& screen_dimensions : list_of_screen_dimensions) {
			print_dimensions(screen_dimensions);

			for (const int percentage_scaler : list_of_percent_scalers) {
				auto scale_by = [](const int dim, const int scaler) {
					return dim * scaler / 100;
				};
				const SDL_Point reduced_dimensions = {
				        scale_by(screen_dimensions.x, percentage_scaler),
				        scale_by(screen_dimensions.y,
				                 percentage_scaler)};

				const auto nearest_dimensions =
				        calc_nearest_aspect_corrected_dimensions(
				                reduced_dimensions, aspect_ratio);

				print_dimensions(nearest_dimensions);
			}
			printf(" %s\n", screen_dimensions.name);
		}

		printf("\n");
	}

	return 0;
}
