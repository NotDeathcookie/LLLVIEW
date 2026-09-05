#include <Geode/Geode.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>
#include <Geode/utils/web.hpp> 
#include <vector>
#include <string>

// FORCE DISABLE MSVC OPTIMIZATIONS TO PREVENT CL.EXE CRASH
#pragma optimize("", off)

using namespace geode::prelude;

std::vector<int> g_lllLevels;
bool g_isLLLSearch = false;
int g_lllPage = 0; 
bool g_isDataLoaded = false;

std::string getChunkedIDString(const std::vector<int>& ids, int pageIndex) {
    size_t start = static_cast<size_t>(pageIndex) * 10;
    if (start >= ids.size()) return "";

    std::string result = "";
    for (size_t i = start; i < start + 10 && i < ids.size(); ++i) {
        if (i > start) result += ",";
        result += std::to_string(ids[i]);
    }
    return result;
}

void processLLLResponse(const std::string& responseString) {
    auto parsed = matjson::parse(responseString);
    if (!parsed) return;
    
    auto arrRes = parsed.unwrap().asArray();
    if (!arrRes) return;
    
    auto arrayData = arrRes.unwrap();
    
    for (auto& entry : arrayData) {
        if (entry.contains("level")) {
            auto levelData = entry["level"];
            if (levelData.contains("levelId")) {
                auto idField = levelData["levelId"];
                if (idField.isNumber()) {
                    g_lllLevels.push_back(idField.asInt().unwrapOr(0));
                } else if (idField.isString()) {
                    try {
                        g_lllLevels.push_back(std::stoi(idField.asString().unwrapOr("0")));
                    } catch (...) {}
                }
            }
        }
    }
    g_isDataLoaded = true;
}

void fetchLLLData() {
    g_lllLevels.clear();
    g_isDataLoaded = false;
    
    async::spawn(
        web::WebRequest().get("https://lcd-community-list.base44.app"),
        [](web::WebResponse response) {
            if (response.ok()) {
                processLLLResponse(response.string().unwrapOr("[]"));
            }
        }
    );
}

$on_mod(Loaded) {
    fetchLLLData();
}

void openLLLBrowser(bool isPush) {
    if (!g_isDataLoaded) {
        FLAlertLayer::create("Loading", "Levels are still being fetched. Please wait.", "OK")->show();
        return;
    }
    if (g_lllLevels.empty()) {
        FLAlertLayer::create("Error", "No levels could be loaded.", "OK")->show();
        return;
    }

    g_isLLLSearch = true;
    std::string searchString = getChunkedIDString(g_lllLevels, g_lllPage);
    auto searchObj = GJSearchObject::create(SearchType::Type19, searchString);
    searchObj->m_page = 0;

    auto scene = LevelBrowserLayer::scene(searchObj);
    if (isPush) {
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));
    } else {
        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(0.5f, scene));
    }
}

class $modify(LLLBrowserLayer, LevelBrowserLayer) {
    bool init(GJSearchObject* searchObj) {
        if (!LevelBrowserLayer::init(searchObj)) return false;
        
        if (g_isLLLSearch) {
            if (auto title = typeinfo_cast<CCLabelBMFont*>(this->getChildByID("title-label"))) {
                title->setString("LCD LEVEL LIST");
                title->setScale(0.7f); 
            }
        }
        return true;
    }

    void setupPageInfo(gd::string info, const char* key) {
        LevelBrowserLayer::setupPageInfo(info, key);

        if (g_isLLLSearch && !g_lllLevels.empty()) {
            int maxPages = (g_lllLevels.size() + 9) / 10;
            int start = (g_lllPage * 10) + 1;
            int end = std::min(static_cast<int>((g_lllPage + 1) * 10), static_cast<int>(g_lllLevels.size()));
            
            std::string pageText = std::to_string(start) + " to " + std::to_string(end) + " of " + std::to_string(g_lllLevels.size());
            
            if (m_countText) m_countText->setString(pageText.c_str());
            if (m_leftArrow) m_leftArrow->setVisible(g_lllPage > 0);
            if (m_rightArrow) m_rightArrow->setVisible(g_lllPage < maxPages - 1);
        }
    }

    void onNextPage(cocos2d::CCObject* sender) {
        if (g_isLLLSearch) {
            int maxPages = (g_lllLevels.size() + 9) / 10;
            if (g_lllPage + 1 < maxPages) {
                g_lllPage++;
                openLLLBrowser(false);
            }
            return;
        }
        LevelBrowserLayer::onNextPage(sender);
    }

    void onPrevPage(cocos2d::CCObject* sender) {
        if (g_isLLLSearch) {
            if (g_lllPage > 0) {
                g_lllPage--;
                openLLLBrowser(false);
            }
            return;
        }
        LevelBrowserLayer::onPrevPage(sender);
    }

    void onBack(CCObject* sender) {
        if (g_isLLLSearch) g_isLLLSearch = false;
        LevelBrowserLayer::onBack(sender);
    }
};

class $modify(LLLLevelSearchLayer, LevelSearchLayer) {
    bool init(int searchType) {
        if (!LevelSearchLayer::init(searchType)) return false;
            
        auto menu = this->getChildByIDRecursive("other-filter-menu");
        if (!menu) menu = this->getChildByIDRecursive("other-filters-menu");
        
        if (menu) {
            if (auto ccMenu = typeinfo_cast<CCMenu*>(menu)) {
                auto spr = CCSprite::createWithSpriteFrameName("difficulty_07_btn2_001.png");
                spr->setScale(0.85f);
                
                auto btn = CCMenuItemSpriteExtra::create(
                    spr, this, menu_selector(LLLLevelSearchLayer::onLLLButton)
                );
                btn->setID("lll-button"_spr);
                ccMenu->addChild(btn);
                ccMenu->updateLayout();
            }
        }
        return true;
    }
    
    void onLLLButton(CCObject*) {
        g_lllPage = 0; 
        openLLLBrowser(true);
    }
};