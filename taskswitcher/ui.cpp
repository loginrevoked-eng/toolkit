#include "ui.hpp"
#include "logging.hpp"
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <shellapi.h>
#include <vector>

namespace SDL_SelectorUI {

namespace {
    constexpr int kIconSize = 20;
    constexpr int kRowPaddingX = 16;
    constexpr int kTextGap = 12;
}

int ListUI::getItemHeight() const {
    if (items.empty()) return windowHeight;
    int itemHeight = windowHeight / static_cast<int>(items.size());
    return itemHeight > 0 ? itemHeight : 1;
}

bool ListUI::loadFont(const std::string& fontPath, int ptsize) {
    font = TTF_OpenFont(fontPath.c_str(), ptsize);
    if (!font) {
        Logging::error("Failed to load font {} error: {}", fontPath, SDL_GetError());
        return false;
    }
    Logging::debug("Font {} loaded successfully", fontPath);
    return true;
}

void ListUI::setItems(const std::vector<std::string>& newItems) {
    items = newItems;
    Logging::debug("items in ListUI has been set total_items:{}", items.size());
}

void ListUI::free() {
    Logging::debug("Freeing ListUI");
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
    items.clear();
}

std::string SwitcherUI::FormatEntry(const WindowInfo& item) {
    return Logging::format("{}: {}", item.processName, wstring_to_string(item.title));
}

void SwitcherUI::SetItems(const std::vector<WindowInfo>& items) {
    windowItems = items;
    listUI.items.clear();
    listUI.items.reserve(windowItems.size());
    for (const auto& item : windowItems) {
        listUI.items.push_back(FormatEntry(item));
    }
    Logging::info("Selector received {} windows", windowItems.size());

    if (listUI.selectedIndex >= static_cast<int>(listUI.items.size())) {
        listUI.selectedIndex = listUI.items.empty() ? 0 : static_cast<int>(listUI.items.size()) - 1;
    }
}

SDL_Surface* SwitcherUI::SurfaceFromHICON(HICON icon, int size) {
    if (!icon || size <= 0) {
        return nullptr;
    }

    HDC screenDC = GetDC(nullptr);
    if (!screenDC) {
        return nullptr;
    }

    HDC memoryDC = CreateCompatibleDC(screenDC);
    if (!memoryDC) {
        ReleaseDC(nullptr, screenDC);
        return nullptr;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) {
            DeleteObject(dib);
        }
        DeleteDC(memoryDC);
        ReleaseDC(nullptr, screenDC);
        return nullptr;
    }

    HGDIOBJ oldBitmap = SelectObject(memoryDC, dib);
    RECT rc{ 0, 0, size, size };
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(memoryDC, &rc, brush);
    DeleteObject(brush);

    DrawIconEx(memoryDC, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL);

    SDL_Surface* surface = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_BGRA32);
    if (surface) {
        if (SDL_LockSurface(surface)) {
            auto* dst = static_cast<std::uint8_t*>(surface->pixels);
            auto* src = static_cast<std::uint8_t*>(bits);
            for (int y = 0; y < size; ++y) {
                std::memcpy(dst + (static_cast<size_t>(y) * surface->pitch),
                    src + (static_cast<size_t>(y) * static_cast<size_t>(size) * 4),
                    static_cast<size_t>(size) * 4);
            }
            SDL_UnlockSurface(surface);
        }
    }

    SelectObject(memoryDC, oldBitmap);
    DeleteObject(dib);
    DeleteDC(memoryDC);
    ReleaseDC(nullptr, screenDC);
    return surface;
}

SDL_Texture* SwitcherUI::GetIconTexture(const WindowInfo& item) {
    if (!windowData.renderer || item.exePath.empty()) {
        return nullptr;
    }

    auto cached = iconCache.find(item.exePath);
    if (cached != iconCache.end()) {
        return cached->second;
    }

    SHFILEINFOW sfi{};
    if (SHGetFileInfoW(item.exePath.c_str(), FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON) == 0) {
        Logging::debug("No icon found for {}", wstring_to_string(item.exePath));
        return nullptr;
    }

    SDL_Texture* texture = nullptr;
    SDL_Surface* surface = SurfaceFromHICON(sfi.hIcon, kIconSize);
    if (surface != nullptr) {
        texture = SDL_CreateTextureFromSurface(windowData.renderer, surface);
        SDL_DestroySurface(surface);
    }
    DestroyIcon(sfi.hIcon);

    if (texture != nullptr) {
        iconCache[item.exePath] = texture;
        Logging::debug("Loaded icon for {}", wstring_to_string(item.exePath));
    }

    return texture;
}

void SwitcherUI::ClearIconCache() {
    for (auto& [path, texture] : iconCache) {
        (void)path;
        if (texture) {
            SDL_DestroyTexture(texture);
        }
    }
    iconCache.clear();
}

std::unique_ptr<ListUI> NewListUI(const std::vector<std::string>& items, int windowWidth, int windowHeight) {
    auto listUI = std::make_unique<ListUI>();
    listUI->items = items;
    listUI->windowWidth = windowWidth;
    listUI->windowHeight = windowHeight;
    Logging::debug("ListUI created with {} items, window size: {}x{}", items.size(), windowWidth, windowHeight);
    return listUI;
}

SwitcherUI::SwitcherUI(const std::string& title, int width, int height, ListUI initialList) {
    listUI = std::move(initialList);
    windowData.title = title;
    windowData.width = width;
    windowData.height = height;
    Logging::debug("SwitcherUI created with title: {}, window size: {}x{}", title, width, height);
}

SwitcherUI::~SwitcherUI() {
    Destroy();
    Logging::debug("SwitcherUI destroyed");
}

void SwitcherUI::InitWindow() {
    if (windowData.alive) return;

    Logging::debug("Initializing SDL");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Logging::panic("Window could not be created");
    }
    sdlInitialized = true;
    Logging::debug("Initializing TTF");
    if (!TTF_Init()) {
        Logging::panic("TTF could not be initialized");
    }
    ttfInitialized = true;

    Logging::debug("Creating window Object(not visible window)");
    windowData.window = SDL_CreateWindow(windowData.title.c_str(), windowData.width, windowData.height, 0);
    if (!windowData.window) {
        Logging::panic("Window could not be created");
    }
    Logging::debug("Creating renderer");
    windowData.renderer = SDL_CreateRenderer(windowData.window, nullptr);
    if (!windowData.renderer) {
        Logging::panic("Renderer could not be created");
    }
    Logging::debug("Getting window ID");
    windowData.id = SDL_GetWindowID(windowData.window);
    Logging::debug("Window ID: {}", windowData.id);
    windowData.alive = true;

    if (!listUI.loadFont()) {
        Logging::panic("Font could not be loaded");
    }
}

void SwitcherUI::Render() {
    if (!windowData.alive) return;

    if (listUI.items.empty()) {
        listUI.selectedIndex = 0;
        listUI.itemChosen = false;
        listUI.chosenIndex = -1;
    } else if (listUI.selectedIndex >= static_cast<int>(listUI.items.size())) {
        listUI.selectedIndex = static_cast<int>(listUI.items.size()) - 1;
    }

    int w, h;
    SDL_GetWindowSize(windowData.window, &w, &h);
    listUI.windowWidth = w;
    listUI.windowHeight = h;

    int itemHeight = listUI.getItemHeight();

    SDL_SetRenderDrawColor(windowData.renderer, 30, 30, 60, 255);
    SDL_RenderClear(windowData.renderer);

    if (listUI.items.empty()) {
        SDL_Color textColor = {230, 230, 240, 255};
        if (listUI.font != nullptr) {
            const char* emptyText = "No windows found";
            SDL_Surface* surface = TTF_RenderText_Blended(listUI.font, emptyText, 0, textColor);
            if (surface != nullptr) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(windowData.renderer, surface);
                if (texture != nullptr) {
                    SDL_FRect textRect = {
                        20.0f,
                        static_cast<float>((listUI.windowHeight - surface->h) / 2),
                        static_cast<float>(surface->w),
                        static_cast<float>(surface->h)
                    };
                    SDL_RenderTexture(windowData.renderer, texture, nullptr, &textRect);
                    SDL_DestroyTexture(texture);
                }
                SDL_DestroySurface(surface);
            }
        } else {
            SDL_RenderDebugText(windowData.renderer, 20.0f, static_cast<float>(listUI.windowHeight / 2), "No windows found");
        }
        SDL_RenderPresent(windowData.renderer);
        return;
    }

    for (int i = 0; i < static_cast<int>(listUI.items.size()); i++) {
        SDL_Texture* iconTexture = (i < static_cast<int>(windowItems.size())) ? GetIconTexture(windowItems[i]) : nullptr;
        SDL_FRect row = {
            0.0f,
            static_cast<float>(i * itemHeight),
            static_cast<float>(listUI.windowWidth),
            static_cast<float>(itemHeight)
        };

        if (i == listUI.selectedIndex) {
            SDL_SetRenderDrawColor(windowData.renderer, 90, 90, 180, 255);
        } else {
            SDL_SetRenderDrawColor(windowData.renderer, 50, 50, 90, 255);
        }
        SDL_RenderFillRect(windowData.renderer, &row);

        SDL_SetRenderDrawColor(windowData.renderer, 20, 20, 40, 255);
        SDL_RenderRect(windowData.renderer, &row);

        float textX = static_cast<float>(kRowPaddingX);
        if (iconTexture != nullptr) {
            SDL_FRect iconRect = {
                static_cast<float>(kRowPaddingX),
                static_cast<float>(i * itemHeight + (itemHeight - kIconSize) / 2),
                static_cast<float>(kIconSize),
                static_cast<float>(kIconSize)
            };
            SDL_RenderTexture(windowData.renderer, iconTexture, nullptr, &iconRect);
            textX += static_cast<float>(kIconSize + kTextGap);
        }

        if (listUI.font != nullptr) {
            SDL_Color textColor = {255, 255, 255, 255};
            SDL_Surface* surface = TTF_RenderText_Blended(listUI.font, listUI.items[i].c_str(), listUI.items[i].size(), textColor);
            if (surface != nullptr) {
                SDL_Texture* texture = SDL_CreateTextureFromSurface(windowData.renderer, surface);
                if (texture != nullptr) {
                    SDL_FRect textRect = {
                        textX,
                        static_cast<float>(i * itemHeight + (itemHeight - surface->h) / 2),
                        static_cast<float>(surface->w),
                        static_cast<float>(surface->h)
                    };
                    SDL_RenderTexture(windowData.renderer, texture, nullptr, &textRect);
                    SDL_DestroyTexture(texture);
                }
                SDL_DestroySurface(surface);
            }
        } else {
            SDL_SetRenderDrawColor(windowData.renderer, 255, 255, 255, 255);
            SDL_RenderDebugText(windowData.renderer, textX, static_cast<float>(i * itemHeight + itemHeight / 2 - 8), listUI.items[i].c_str());
        }
    }

    SDL_RenderPresent(windowData.renderer);
}

void SwitcherUI::Destroy() {
    ClearIconCache();
    if (windowData.alive) {
        SDL_DestroyRenderer(windowData.renderer);
        SDL_DestroyWindow(windowData.window);
        windowData.window = nullptr;
        windowData.renderer = nullptr;
        windowData.id = 0;
        windowData.alive = false;
    }
    if (listUI.font != nullptr) {
        TTF_CloseFont(listUI.font);
        listUI.font = nullptr;
    }
    if (ttfInitialized) {
        TTF_Quit();
        ttfInitialized = false;
    }
    if (sdlInitialized) {
        SDL_Quit();
        sdlInitialized = false;
    }
}

void SwitcherUI::SelectViaKeyBoard(int key, ListUI& listUI)
{
    switch (key) {
        case SDLK_UP:
            listUI.selectedIndex = (listUI.selectedIndex - 1 + static_cast<int>(listUI.items.size())) % static_cast<int>(listUI.items.size());
            break;
        case SDLK_DOWN:
            listUI.selectedIndex = (listUI.selectedIndex + 1) % static_cast<int>(listUI.items.size());
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            listUI.itemChosen = true;
            listUI.chosenIndex = listUI.selectedIndex;
            break;
    }
}

void SwitcherUI::SelectViaMouth(int clickX, int clickY, ListUI& listUI) 
{
    (void)clickX;
    int itemHeight = listUI.getItemHeight();
    if (itemHeight <= 0) {
        return;
    }
    int selectedIndex = clickY / itemHeight;
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(listUI.items.size())) {
        listUI.selectedIndex = selectedIndex;
        listUI.itemChosen = true;
        listUI.chosenIndex = selectedIndex;
    }
}

void SwitcherUI::SelectHoveredItem(float mouseX, float mouseY, ListUI& listUI) 
{
    float mx = mouseX;
    float my = mouseY;
    int itemHeight = listUI.getItemHeight();

    if (mx >= 0 && mx <= listUI.windowWidth && my >= 0) {
        int index = static_cast<int>(my) / itemHeight;
        if (index >= 0 && index < static_cast<int>(listUI.items.size())) {
            listUI.selectedIndex = index;
        }
    }
}

void SwitcherUI::HandleEvents(SDL_Event* event) 
{
    if (event == nullptr) return;

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == windowData.id) {
        Destroy();
        return;
    }

    if (listUI.items.empty()) return;

    if (event->type == SDL_EVENT_KEY_DOWN){
        SelectViaKeyBoard(event->key.key, listUI);
    }
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        SelectViaMouth(event->button.x, event->button.y, listUI);
    }
    else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        SelectHoveredItem(event->motion.x, event->motion.y, listUI);
    }
}

bool SwitcherUI::ShouldQuit() {
    return !windowData.alive;
}

void SwitcherUI::FocusKeyboard() {
    if (!windowData.alive || !windowData.window) return;

    SDL_PropertiesID props = SDL_GetWindowProperties(windowData.window);
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL));

    if (hwnd) {
        SetFocus(hwnd);
        SetForegroundWindow(hwnd);
    }
}

int SwitcherUI::StartLoop(std::vector<WindowInfo>* items) {
    Logging::debug("StartLoop() called");

    if (items != nullptr) {
        SetItems(*items);
        std::cout << "Updated listUI with " << items->size() << " items" << std::endl;
    } else {
        std::cout << "items is null, using existing items" << std::endl;
    }

    InitWindow();
    FocusKeyboard();
    Logging::info("Selector window opened with {} items", listUI.items.size());
    Render();

    SDL_Event event;
    int chosenIndex = -1;

    while (!ShouldQuit()) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                Destroy();
                return -1;
            }

            HandleEvents(&event);

            if (!ShouldQuit()) {
                Render();
            }

            if (listUI.itemChosen) {
                std::cout << "\n=== ITEM CHOSEN ===" << std::endl;
                std::cout << "Index: " << listUI.chosenIndex << std::endl;
                std::cout << "Item: " << listUI.items[listUI.chosenIndex] << std::endl;
                std::cout << "==================\n" << std::endl;
                Logging::info("Selector confirmed index {}", listUI.chosenIndex);

                chosenIndex = listUI.chosenIndex;
                listUI.itemChosen = false;
                Destroy();
                return chosenIndex;
            }

            if (ShouldQuit()) break;
        }
        SDL_Delay(10);
    }
    Destroy();
    return chosenIndex;
}

ListUI& SwitcherUI::GetListUI() {
    return listUI;
}

} // namespace SDL_SelectorUI
