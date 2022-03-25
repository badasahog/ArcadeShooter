#include <Windows.h>
#include <wrl.h>
#include <d2d1.h>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <source_location>
#include <wincodec.h>
#include <ctime>
#include <chrono>
#include <cmath>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "windowscodecs")

#pragma region("preprocessor system checks")
#ifdef _DEBUG
#ifdef NDEBUG
#error debug flag conflict
#endif
#endif

#if !_HAS_CXX20
#ifdef __clang__
#error clang does not yet have full C++20 support. To run this application on msbuild please select c++latest as the version
#endif
#error C++20 is required
#endif

#ifndef _M_AMD64
#error only x64 architectures are supported, please select x64 in your build configuration
#endif

#ifndef __has_cpp_attribute
#error critical macro __has_cpp_attribute not defined
#endif

#if !__has_include(<Windows.h>)
#error critital header Windows.h not found
#endif

#pragma endregion("preprocessor system checks")
#pragma region("macros")

#ifdef min
#pragma push_macro("min")
#undef min
#endif

#ifdef max
#pragma push_macro("max")
#undef max
#endif

#ifdef CreateWindow
#pragma push_macro("CreateWindow")
#undef CreateWindow
#endif

#ifdef _DEBUG
#define BDEBUG 1
#else
#define BDEBUG 0
#endif

#if __has_cpp_attribute(nodiscard) < 201603L
#define _NODISCARD [[nodiscard]]
#endif

#ifndef DECLSPEC_NOINLINE
#if _MSC_VER >= 1300
#define DECLSPEC_NOINLINE  __declspec(noinline)
#else
#define DECLSPEC_NOINLINE
#endif
#endif

#ifndef _NORETURN
#define _NORETURN [[noreturn]]
#endif

#pragma endregion("macros")
#pragma region("function macros")

#ifdef _DEBUG
#define ASSERT_SUCCESS(x) assert(SUCCEEDED(x))
#else
#define ASSERT_SUCCESS(x) x
#endif

#ifdef __clang__ // clang gives error when using std::source_location
#define THROW_ON_FAIL(x) \
	if (FAILED(x)){ \
		std::cout << std::dec << "[" << __LINE__ <<"]ERROR: " << std::hex << x << std::dec << '\n'; \
		throw std::exception(); \
	}
#else
#define THROW_ON_FAIL(x) \
	if (FAILED(x)){ \
		std::cout << std::dec << "[" << std::source_location::current().line() <<"]ERROR: " << std::hex << x << std::dec << '\n'; \
		throw std::exception(); \
	}
#endif

#pragma endregion("function macros")
#pragma endregion("preprocessor")

using Microsoft::WRL::ComPtr;

constexpr int OFFSET_X = 25;
constexpr int OFFSET_Y = 25;
constexpr int RESOLUTION_X = 256;
constexpr int RESOLUTION_Y = 224;
/*
* research:
* in a closed stadium player x goes from 30 to 226
* in a closed stadium player y goes from 49 to 222
* 
* in any stadium bullet x goes from 18 to 238
* in any stadium bullet y goes from 20 to 220
* 
* player torso is offset from position by -16, -24
* 
* player pants is offset from position by -8, -8
* 
* when facing left the sprites are shifted to the right by 1
* 
* when shooting, a bullet is fired every 9 frames
* 
* bullet offsets (first frame after firing):
* 
* up: 0, -28
* 
* up left: -12, -26
* 
* left: -20, -14
* 
* down left: -12, -1
* 
* down: 1, 4
* 
* down right: 11, -1
* 
* right: 21, -14
* 
* up right: 11, -26
* 
* bullets fired diagonally alternate between movements of 3 and 4 in each direction.
* when a bullet hits the wall, it stays there for six frames.
* 
* when walking up left and shooting up right, the hero's legs are shifted one pixel to the left
* 
* 
* Enemies have 3 AI values that I'm aware of:
* 000D82:
* when frozen the enemy will only change directions when hitting a wall. If this happens, the enemy will always choose the most efficient direction to reach the hero
*/
LRESULT CALLBACK WindowProc(HWND _In_ hwnd, UINT _In_ uMsg, WPARAM _In_ wParam, LPARAM _In_ lParam);

ComPtr<ID2D1Factory> m_pDirect2dFactory;
ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
ComPtr<ID2D1SolidColorBrush> m_pLightSlateGrayBrush;
ComPtr<ID2D1SolidColorBrush> m_pCornflowerBlueBrush;

struct controller_t
{
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool A = false;
	bool B = false;
	bool X = false;
	bool Y = false;
};
controller_t controller;

struct bmpResourceSet
{
	ComPtr<IWICBitmapDecoder> wicDecoder;
	ComPtr<IWICBitmapFrameDecode> wicFrame;
	ComPtr<IWICFormatConverter> wicConverter;
	ComPtr<ID2D1Bitmap> bmp;
	const wchar_t* const filename;

	bmpResourceSet(const wchar_t* const par_filename) :
		filename{ par_filename }
	{
	}


};
int heroX = 50, heroY = 100;
int main()
{
	static const WCHAR msc_fontName[] = L"Verdana";
	static const FLOAT msc_fontSize = 50;

	std::vector<bmpResourceSet> bmpResourceSets
	{{
		L"sprites/hero_torso_U.png",
		L"sprites/hero_torso_UL.png",
		L"sprites/hero_torso_L.png",
		L"sprites/hero_torso_DL.png",
		L"sprites/hero_torso_D.png",
		L"sprites/hero_torso_DR.png",
		L"sprites/hero_torso_R.png",
		L"sprites/hero_torso_UR.png",
		L"sprites/hero_legs_stand_D.png",
		L"sprites/hero_legs_stand_UL.png",
		L"sprites/hero_legs_stand_L.png",
		L"sprites/hero_legs_stand_DL.png",
		L"sprites/hero_legs_stand_D.png",
		L"sprites/hero_legs_stand_DR.png",
		L"sprites/hero_legs_stand_R.png",
		L"sprites/hero_legs_stand_UR.png",
		L"sprites/hero_legs_walk_L_1.png",
		L"sprites/hero_legs_walk_L_2.png",
		L"sprites/hero_legs_walk_L_3.png",
		L"sprites/hero_legs_walk_L_4.png",
		L"sprites/hero_legs_walk_L_5.png",
		L"sprites/hero_legs_walk_L_6.png",
		L"sprites/hero_legs_walk_L_7.png",
		L"sprites/hero_legs_walk_D_1.png",
		L"sprites/hero_legs_walk_D_2.png",
		L"sprites/hero_legs_walk_D_3.png",
		L"sprites/hero_legs_walk_D_4.png",
		L"sprites/hero_legs_walk_D_5.png",
		L"sprites/hero_legs_walk_D_6.png",
		L"sprites/hero_legs_walk_DL_1.png",
		L"sprites/hero_legs_walk_DL_2.png",
		L"sprites/hero_legs_walk_DL_3.png",
		L"sprites/hero_legs_walk_DL_4.png",
		L"sprites/hero_legs_walk_DL_5.png",
		L"sprites/hero_legs_walk_DL_6.png",
		L"sprites/hero_legs_walk_DL_7.png",
		L"sprites/hero_legs_walk_UL_1.png",
		L"sprites/hero_legs_walk_UL_2.png",
		L"sprites/hero_legs_walk_UL_3.png",
		L"sprites/hero_legs_walk_UL_4.png",
		L"sprites/hero_legs_walk_UL_5.png",
		L"sprites/hero_legs_walk_UL_6.png",
		L"sprites/hero_legs_walk_R_1.png",
		L"sprites/hero_legs_walk_R_2.png",
		L"sprites/hero_legs_walk_R_3.png",
		L"sprites/hero_legs_walk_R_4.png",
		L"sprites/hero_legs_walk_R_5.png",
		L"sprites/hero_legs_walk_R_6.png",
		L"sprites/hero_legs_walk_R_7.png",
		L"sprites/hero_legs_walk_UR_1.png",
		L"sprites/hero_legs_walk_UR_2.png",
		L"sprites/hero_legs_walk_UR_3.png",
		L"sprites/hero_legs_walk_UR_4.png",
		L"sprites/hero_legs_walk_UR_5.png",
		L"sprites/hero_legs_walk_UR_6.png",
		L"sprites/hero_legs_walk_DR_1.png",
		L"sprites/hero_legs_walk_DR_2.png",
		L"sprites/hero_legs_walk_DR_3.png",
		L"sprites/hero_legs_walk_DR_4.png",
		L"sprites/hero_legs_walk_DR_5.png",
		L"sprites/hero_legs_walk_DR_6.png",
		L"sprites/hero_legs_walk_DR_7.png",
		L"sprites/hero_shoot_U_1.png",
		L"sprites/hero_shoot_U_2.png",
		L"sprites/hero_shoot_UL_1.png",
		L"sprites/hero_shoot_UL_2.png",
		L"sprites/hero_shoot_L_1.png",
		L"sprites/hero_shoot_L_2.png",
		L"sprites/hero_shoot_DL_1.png",
		L"sprites/hero_shoot_DL_2.png",
		L"sprites/hero_shoot_D_1.png",
		L"sprites/hero_shoot_D_2.png",
		L"sprites/hero_shoot_DR_1.png",
		L"sprites/hero_shoot_DR_2.png",
		L"sprites/hero_shoot_R_1.png",
		L"sprites/hero_shoot_R_2.png",
		L"sprites/hero_shoot_UR_1.png",
		L"sprites/hero_shoot_UR_2.png",
		L"sprites/background.png",
		L"sprites/hero_bullet_U.png",
		L"sprites/hero_bullet_UL.png",
		L"sprites/hero_bullet_L.png",
		L"sprites/hero_bullet_DL.png",
		L"sprites/hero_bullet_D.png",
		L"sprites/hero_bullet_DR.png",
		L"sprites/hero_bullet_R.png",
		L"sprites/hero_bullet_UR.png",
		L"sprites/hero_bullet_explode_U.png",
		L"sprites/hero_bullet_explode_L.png",
		L"sprites/hero_bullet_explode_D.png",
		L"sprites/hero_bullet_explode_R.png",
	}};
	HINSTANCE hInstance = GetModuleHandleW(nullptr);
	// Register the window class.
	const wchar_t CLASS_NAME[] = L"Sample Window Class";

	WNDCLASSEXW wc = { 
		.lpfnWndProc = WindowProc,
		.hInstance = hInstance,
		.lpszClassName = CLASS_NAME,
		.cbSize = sizeof(wc)
	};
	RegisterClassExW(&wc);
	// Create the window.
	HWND hwnd = CreateWindowExW(
		0,
		CLASS_NAME,
		L"Smash TV",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (hwnd == NULL)
	{
		return 0;
	}

	IWICImagingFactory* wicFactory = nullptr;

	CoInitializeEx(
		nullptr,
		COINIT_MULTITHREADED
	);
	THROW_ON_FAIL(CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IWICImagingFactory,
		(LPVOID*)&wicFactory
	));
	//IWICBitmapDecoder* wicDecoder = nullptr;

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_pDirect2dFactory.GetAddressOf());


	D2D1_SIZE_U size = D2D1::SizeU(
		RESOLUTION_X,
		RESOLUTION_Y
	);

	// Create a Direct2D render target.
	THROW_ON_FAIL(m_pDirect2dFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(hwnd, size),
		&m_pRenderTarget
	));

	m_pRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::LightSlateGray),
		&m_pLightSlateGrayBrush
	);

	m_pRenderTarget->CreateSolidColorBrush(
		D2D1::ColorF(D2D1::ColorF::CornflowerBlue),
		&m_pCornflowerBlueBrush
	);

	// Create a DirectWrite factory.
	//hr = DWriteCreateFactory(
	//	DWRITE_FACTORY_TYPE_SHARED,
	//	__uuidof(m_pDWriteFactory),
	//	reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
	//);
	//
	//hr = m_pDWriteFactory->CreateTextFormat(
	//	msc_fontName,
	//	NULL,
	//	DWRITE_FONT_WEIGHT_NORMAL,
	//	DWRITE_FONT_STYLE_NORMAL,
	//	DWRITE_FONT_STRETCH_NORMAL,
	//	msc_fontSize,
	//	L"", //locale
	//	&m_pTextFormat
	//);
	//
	//m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	//
	//m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	const int vectorSize = bmpResourceSets.size();
	for (int i = 0; i < vectorSize;i++)//auto&& resourceSet : bmpResourceSets)
	{
		THROW_ON_FAIL(wicFactory->CreateDecoderFromFilename
		(
			bmpResourceSets[i].filename,
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			&bmpResourceSets[i].wicDecoder
		));

		//IWICBitmapFrameDecode* wicFrame = nullptr;
		THROW_ON_FAIL(bmpResourceSets[i].wicDecoder->GetFrame(0, &bmpResourceSets[i].wicFrame));



		ComPtr<IWICBitmapFlipRotator> flipper;
		THROW_ON_FAIL(wicFactory->CreateBitmapFlipRotator(flipper.GetAddressOf()));
		THROW_ON_FAIL(flipper->Initialize(bmpResourceSets[i].wicFrame.Get(), WICBitmapTransformFlipHorizontal));
		//IWICFormatConverter* wicConverter = nullptr;

		THROW_ON_FAIL(wicFactory->CreateFormatConverter(&bmpResourceSets[i].wicConverter));

		THROW_ON_FAIL(bmpResourceSets[i].wicConverter->Initialize(
			bmpResourceSets[i].wicFrame.Get(),
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom
		));
		THROW_ON_FAIL(m_pRenderTarget->CreateBitmapFromWicBitmap(
			bmpResourceSets[i].wicConverter.Get(),
			NULL,
			bmpResourceSets[i].bmp.GetAddressOf()
		));


		//create the mirrored sprite
	}


	ShowWindow(hwnd, true);
	auto lastFrame = std::chrono::system_clock::now();
	uint64_t frameNum = 0;
	int state = 0;
	struct bullet
	{
		int x;
		int y;
		int direction;
		int burnout; //
	};
	std::vector<bullet> bullets;
	/*
	* 
	*     0
	*   1   7
	*  2     6
	*   3   5
	*     4
	* 
	* 
	*/
	MSG msg = { };
	bool esc = false;
	while (!esc)
	{
		//UpdateWindow(hwnd);
		if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) {
				esc = true;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);

			//std::cout << msg.message << std::endl;
		}
		else
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - lastFrame).count() > (1000/60))
			{
				lastFrame = std::chrono::system_clock::now();
				frameNum++;
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);

				if (controller.Y && controller.X)
					state = 1;
				else if (controller.Y && controller.B)
					state = 3;
				else if (controller.A && controller.B)
					state = 5;
				else if (controller.X && controller.A)
					state = 7;
				else if (controller.Y)
					state = 2;
				else if (controller.X)
					state = 0;
				else if (controller.B)
					state = 4;
				else if (controller.A)
					state = 6;
				else if (controller.left && controller.up)
					state = 1;
				else if (controller.left && controller.down)
					state = 3;
				else if (controller.right && controller.down)
					state = 5;
				else if (controller.up && controller.right)
					state = 7;
				else if (controller.left)
					state = 2;
				else if (controller.up)
					state = 0;
				else if (controller.down)
					state = 4;
				else if (controller.right)
					state = 6;
				const bool isShooting = controller.A || controller.B || controller.X || controller.Y;
				if (isShooting && frameNum % 7 == 0)
				{
					if (state == 0)
						bullets.push_back({
							.x = heroX - 0,
							.y = heroY -28,
							.direction = state,
							.burnout = 0
						});
					else if (state == 1)
						bullets.push_back({
							.x = heroX - 12,
							.y = heroY - 26,
							.direction = state,
							.burnout = 0
						});
					else if (state == 2)
						bullets.push_back({
							.x = heroX - 20,
							.y = heroY - 14,
							.direction = state,
							.burnout = 0
						});
					else if (state == 3)
						bullets.push_back({
							.x = heroX - 12,
							.y = heroY - 1,
							.direction = state,
							.burnout = 0
						});
					else if (state == 4)
						bullets.push_back({
							.x = heroX + 1,
							.y = heroY + 4,
							.direction = state,
							.burnout = 0
						});
					else if (state == 5)
						bullets.push_back({
							.x = heroX + 11,
							.y = heroY - 1,
							.direction = state,
							.burnout = 0
						});
					else if (state == 6)
						bullets.push_back({
							.x = heroX + 21,
							.y = heroY - 14,
							.direction = state,
							.burnout = 0
						});
					else if (state == 7)
						bullets.push_back({
							.x = heroX + 11,
							.y = heroY - 26,
							.direction = state,
							.burnout = 0
						});
				}

				m_pRenderTarget->BeginDraw();

				m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));
				m_pRenderTarget->DrawBitmap(bmpResourceSets.at(78).bmp.Get(),
					D2D1::RectF
					(
						0,
						0,
						0 + bmpResourceSets.at(78).bmp->GetSize().width,
						0 + bmpResourceSets.at(78).bmp->GetSize().height
					), 1.0f,
					D2D1_BITMAP_INTERPOLATION_MODE::D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
					D2D1::RectF(
						0.0f, 0.0f,
						bmpResourceSets.at(78).bmp->GetSize().width,
						bmpResourceSets.at(78).bmp->GetSize().height
					));
				m_pRenderTarget->DrawBitmap
				(
					bmpResourceSets.at(
						[&]()->int {
							if ((controller.up && controller.left) || (controller.down && controller.right && state == 1))
								return (frameNum / 4) % 6 + 36;
							else if ((controller.up && controller.right && state != 3) || (controller.down && controller.left && state == 7))
								return (frameNum / 4) % 6 + 43;
							else if ((controller.down && controller.left) || (controller.up && controller.right && state == 3))
								return (frameNum / 4) % 7 + 29;
							else if ((controller.down && controller.right) || (controller.up && controller.left && state == 5))
								return (frameNum / 4) % 7 + 49;
							else if ((controller.left && state == 2) || (controller.right && state == 2))
								return (frameNum / 4) % 7 + 16;
							else if (controller.down || controller.up)
								return (frameNum / 4) % 6 + 23;
							else if (controller.right || (controller.left && state == 6))
								return (frameNum / 4) % 7 + 42;

							return state + 8;
						}()).bmp.Get(),
					D2D1::RectF
					(
						heroX - 8,
						heroY - 8,
						heroX - 8 + bmpResourceSets.at(1).bmp->GetSize().width,
						heroY - 8 + bmpResourceSets.at(1).bmp->GetSize().height
					),
					1.0f,
					D2D1_BITMAP_INTERPOLATION_MODE::D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
					D2D1::RectF(
						0.0f, 0.0f,
						bmpResourceSets.at(1).bmp->GetSize().width,
						bmpResourceSets.at(1).bmp->GetSize().height
					)
				);

				m_pRenderTarget->DrawBitmap
				(
					bmpResourceSets.at(
						[&]()->int {
							if (isShooting)
								return 62 + 2 * state + (frameNum / 4) % 2;
							else return state;
						}()).bmp.Get(),
					D2D1::RectF
					(
						heroX - 16,
						heroY - 24,
						heroX - 16 + bmpResourceSets.at(0).bmp->GetSize().width,
						heroY - 24 + bmpResourceSets.at(0).bmp->GetSize().height
					),
					1.0f,
					D2D1_BITMAP_INTERPOLATION_MODE::D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
					D2D1::RectF(
						0.0f, 0.0f,
						bmpResourceSets.at(0).bmp->GetSize().width, bmpResourceSets.at(0).bmp->GetSize().height
					)
				);
				for (int i = bullets.size()-1; i > -1; i--)
				{
					m_pRenderTarget->DrawBitmap
					(
						bmpResourceSets.at(
							[&]()->int {
								if (bullets[i].burnout == 0)
									return bullets[i].direction + 79;
								else return 87 + bullets[i].direction/2;
						}()).bmp.Get(),
						D2D1::RectF
						(
							bullets[i].x - 8,
							bullets[i].y - 8,
							bullets[i].x - 8 + bmpResourceSets.at(0).bmp->GetSize().width,
							bullets[i].y - 8 + bmpResourceSets.at(0).bmp->GetSize().height
						),
						1.0f,
						D2D1_BITMAP_INTERPOLATION_MODE::D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
						D2D1::RectF(
							0.0f, 0.0f,
							bmpResourceSets.at(0).bmp->GetSize().width, bmpResourceSets.at(0).bmp->GetSize().height
						)
					);

					if (bullets[i].burnout == 0)
					{
						if (bullets[i].direction == 0)
							bullets[i].y -= 5;
						else if (bullets[i].direction == 2)
							bullets[i].x -= 5;
						else if (bullets[i].direction == 4)
							bullets[i].y += 5;
						else if (bullets[i].direction == 6)
							bullets[i].x += 5;

						if (bullets[i].direction == 1)
						{
							bullets[i].x -= frameNum % 2 == 0 ? 3 : 4;
							bullets[i].y -= frameNum % 2 == 1 ? 3 : 4;
						}
						else if (bullets[i].direction == 3)
						{
							bullets[i].x -= frameNum % 2 == 0 ? 3 : 4;
							bullets[i].y += frameNum % 2 == 1 ? 3 : 4;
						}
						else if (bullets[i].direction == 5)
						{
							bullets[i].x += frameNum % 2 == 0 ? 3 : 4;
							bullets[i].y += frameNum % 2 == 1 ? 3 : 4;
						}
						else if (bullets[i].direction == 7)
						{
							bullets[i].x += frameNum % 2 == 0 ? 3 : 4;
							bullets[i].y -= frameNum % 2 == 1 ? 3 : 4;
						}
					}
					else if (bullets[i].burnout == 6)
					{
						bullets.erase(bullets.begin() + i);
						continue;
					}
					else
					{
						bullets[i].burnout++;
					}

					//check bounds

					//in any stadium bullet x goes from 18 to 238
					//in any stadium bullet y goes from 20 to 220
					if (bullets[i].burnout == 0)
					{
						if (bullets[i].x < 18)
							bullets[i].x = 18, bullets[i].burnout = 1, bullets[i].direction = 2;
						else if (bullets[i].x > 238)
							bullets[i].x = 238, bullets[i].burnout = 1, bullets[i].direction = 6;
						else if (bullets[i].y < 20)
							bullets[i].y = 20, bullets[i].burnout = 1, bullets[i].direction = 0;
						else if (bullets[i].y > 220)
							bullets[i].y = 220, bullets[i].burnout = 1, bullets[i].direction = 4;
					}
				}
				
				//m_pRenderTarget->DrawText(
				//	sc_helloWorld,
				//	ARRAYSIZE(sc_helloWorld) - 1,
				//	m_pTextFormat,
				//	D2D1::RectF(0, 0, renderTargetSize.width, renderTargetSize.height),
				//	m_pBlackBrush
				//);

				m_pRenderTarget->EndDraw();
				EndPaint(hwnd, &ps);


				if (controller.up)
					heroY--;
				else if (controller.down)
					heroY++;
				if (controller.left)
					heroX--;
				else if (controller.right)
					heroX++;

				if (heroX < 30)heroX = 30;
				if (heroY < 49)heroY = 49;
				if (heroX > 226)heroX = 226;
				if (heroY > 222)heroY = 222;


			}
		}

	}
	return 0;
}

LRESULT CALLBACK WindowProc(HWND _In_ hwnd, UINT _In_ uMsg, WPARAM _In_ wParam, LPARAM _In_ lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
		break;
	case WM_KEYDOWN:
		if (wParam == VK_SPACE)
			std::cout << "keys and shit" << std::endl;
		else if (wParam == VK_VOLUME_DOWN)
			std::cout << "keyf" << std::endl;
		else if (wParam == 'D')
			controller.right = true;
		else if (wParam == 'A')
			controller.left = true;
		else if (wParam == 'W')
			controller.up = true;
		else if (wParam == 'S')
			controller.down = true;
		else if (wParam == VK_UP)
			controller.X = true;
		else if (wParam == VK_LEFT)
			controller.Y = true;
		else if (wParam == VK_DOWN)
			controller.B = true;
		else if (wParam == VK_RIGHT)
			controller.A = true;
		break;
	case WM_KEYUP:
		if (wParam == 'D')
			controller.right = false;
		else if (wParam == 'A')
			controller.left = false;
		else if (wParam == 'W')
			controller.up = false;
		else if (wParam == 'S')
			controller.down = false;
		else if (wParam == VK_UP)
			controller.X = false;
		else if (wParam == VK_LEFT)
			controller.Y = false;
		else if (wParam == VK_DOWN)
			controller.B = false;
		else if (wParam == VK_RIGHT)
			controller.A = false;
		break;
		return 0;

	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}