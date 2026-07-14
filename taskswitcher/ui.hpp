#ifndef SDL_SELECTOR_UI_HPP
#define SDL_SELECTOR_UI_HPP

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <windows.h>

namespace SDL_SelectorUI {

struct ListUI {
    std::vector<std::string> items;
    int selectedIndex = 0;
    int windowWidth = 640;
    int windowHeight = 480;
    TTF_Font* font = nullptr;
    bool itemChosen = false;
    int chosenIndex = -1;

    int getItemHeight() const;
    bool loadFont(const std::string& fontPath = "C:/Windows/Fonts/arial.ttf", int ptsize = 32);
    void setItems(const std::vector<std::string>& newItems);
    void free();
};

struct WindowData {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    Uint32 id = 0;
    bool alive = false;
    std::string title;
    int width = 0;
    int height = 0;
};

std::unique_ptr<ListUI> NewListUI(const std::vector<std::string>& items, int windowWidth, int windowHeight);

class SwitcherUI {
private:
    WindowData windowData;
    ListUI listUI;


public:
    SwitcherUI(const std::string& title, int width, int height, ListUI initialList);
    ~SwitcherUI();

    void InitWindow();
    void Render(std::vector<std::string>* items = nullptr);
    void Destroy();
    void HandleEvents(SDL_Event* event);
    bool ShouldQuit();
    void FocusKeyboard();
    void StartLoop(std::vector<std::string>* items = nullptr);
    void SelectViaKeyBoard(int key, ListUI& listUI);
    void SelectViaMouth(int x, int y, ListUI& listUI);
    void SelectHoveredItem(float x, float y, ListUI& listUI);
    ListUI& GetListUI();
};

} // namespace SDL_SelectorUI

#endif // SDL_SELECTOR_UI_HPP