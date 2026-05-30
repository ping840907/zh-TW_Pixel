#include <pebble.h>

// ==================== 常數定義 ====================

// 動畫參數
#define ANIMATION_DURATION_MS 300
#define ANIMATION_OFFSET_Y 5

// 特殊資源 ID 標記
#define RESOURCE_ID_NONE 0

// 平台相關佈局
#if defined(PBL_PLATFORM_EMERY)
    #define TIME_IMAGE_SIZE GSize(88, 88)
    #define DATE_IMAGE_SIZE GSize(22, 22)
    #define TIME_COL1_X 8
    #define TIME_COL2_X 104
    #define TIME_ROW1_Y 8
    #define TIME_ROW2_Y 104
    #define DATE_ROW_Y 199
    #define DATE_MONTH1_X 8
    #define DATE_MONTH2_X 31
    #define DATE_YUE_X 54
    #define DATE_DAY1_X 77
    #define DATE_DAY2_X 100
    #define DATE_RI_X 123
    #define DATE_ZHOU_X 146
    #define DATE_WEEK_X 169
    // 月日合併為單一字串後，於此區域內水平置中（字與字間距 1px）
    #define DATE_MD_REGION_X 0
    #define DATE_MD_REGION_W 144
    #define DATE_GLYPH_GAP 1
#else
    #define TIME_IMAGE_SIZE GSize(66, 66)
    #define DATE_IMAGE_SIZE GSize(11, 11)
    #define TIME_COL1_X 4
    #define TIME_COL2_X 74
    #define TIME_ROW1_Y 4
    #define TIME_ROW2_Y 74
    #define DATE_ROW_Y 149
    #define DATE_MONTH1_X 4
    #define DATE_MONTH2_X 16
    #define DATE_YUE_X 28
    #define DATE_DAY1_X 50
    #define DATE_DAY2_X 62
    #define DATE_RI_X 74
    #define DATE_ZHOU_X 117
    #define DATE_WEEK_X 129
#endif

// ==================== 列舉與類型定義 ====================

// 設定鍵值
typedef enum {
    KEY_HOUR_COLOR = 0,
    KEY_MINUTE_COLOR = 1,
    KEY_THEME_IS_DARK = 2,
    KEY_ANIMATION_ENABLED = 3,
    KEY_BACKGROUND_COLOR = 4,
    KEY_TEXT_COLOR = 5,
    KEY_BW_HOUR_ACCENT = 6,

} SettingKey;

// 圖層類型（用於主題應用）
typedef enum {
    LAYER_TYPE_HOUR,
    LAYER_TYPE_MINUTE_ACCENT,
    LAYER_TYPE_MINUTE_NORMAL,
    LAYER_TYPE_DATE,
    LAYER_TYPE_STATIC,
} LayerType;

// 動畫狀態
typedef enum {
    ANIM_STATE_IDLE,
    ANIM_STATE_FADE_OUT,
    ANIM_STATE_FADE_IN,
} AnimationState;

// 顯示圖層結構
typedef struct {
    BitmapLayer *layer;
    GBitmap *bitmap;
    uint32_t current_resource_id;
    PropertyAnimation *animation;
    AnimationState anim_state;
    GRect base_frame;
    LayerType type;
} DisplayLayer;

// 主題配置
typedef struct {
    GColor background;
    GColor text;
    GColor hour_accent;
    GColor minute_accent;
    // 黑白平台專用設定（彩色平台直接由 AppMessage 寫入，不使用這兩個欄位）
    bool is_dark;
    bool bw_hour_accent;
} ThemeConfig;

// 應用狀態
typedef struct {
    Window *main_window;
    ThemeConfig theme;
    bool animation_enabled;

    DisplayLayer hour_layers[2];
    DisplayLayer minute_layers[2];
    DisplayLayer month_layers[2];
    DisplayLayer day_layers[2];
    DisplayLayer week_layer;
    DisplayLayer yue_layer;
    DisplayLayer ri_layer;
    DisplayLayer zhou_layer;

    // 日期區塊上次顯示的數值，用於偵測「月／日／週」各區塊是否變動以觸發跳動動畫。
    // 初始化為 -1（不可能的數值），確保初次載入時三個區塊都會播放動畫。
    int last_month;
    int last_day;
    int last_week;
} AppState;

// ==================== 全域狀態 ====================

static AppState s_app;

// ==================== 資源映射表 ====================
//
// 每個陣列將數字索引對應至圖片資源 ID，供時間與日期圖層查表使用。
// 命名規則：
//   UPPERCASE（U）  = 大號大寫中文數字圖片，用於顯示「時」
//   LOWERCASE（L）  = 大號小寫中文數字圖片，用於顯示「分」
//   DATE_UPPERCASE（SU）/ DATE_LOWERCASE（SL）= 同上的縮小版，用於顯示「月日」
//   TENS = 十位圖，ONES = 個位圖
//   RESOURCE_ID_NONE = 該位置不顯示圖片（例如小時十位不足時）

static const uint32_t TIME_UPPERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_NONE, RESOURCE_ID_IMG_U10, RESOURCE_ID_IMG_U2,
};

static const uint32_t TIME_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_U10, RESOURCE_ID_IMG_U1, RESOURCE_ID_IMG_U2, RESOURCE_ID_IMG_U3,
    RESOURCE_ID_IMG_U4, RESOURCE_ID_IMG_U5, RESOURCE_ID_IMG_U6, RESOURCE_ID_IMG_U7,
    RESOURCE_ID_IMG_U8, RESOURCE_ID_IMG_U9
};

static const uint32_t TIME_LOWERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_IMG_L0, RESOURCE_ID_IMG_L10, RESOURCE_ID_IMG_L20, RESOURCE_ID_IMG_L30,
    RESOURCE_ID_IMG_L4, RESOURCE_ID_IMG_L5,
};

static const uint32_t TIME_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_L10, RESOURCE_ID_IMG_L1, RESOURCE_ID_IMG_L2, RESOURCE_ID_IMG_L3,
    RESOURCE_ID_IMG_L4, RESOURCE_ID_IMG_L5, RESOURCE_ID_IMG_L6, RESOURCE_ID_IMG_L7,
    RESOURCE_ID_IMG_L8, RESOURCE_ID_IMG_L9
};

static const uint32_t DATE_UPPERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SU10, RESOURCE_ID_IMG_SU1, RESOURCE_ID_IMG_SU2, RESOURCE_ID_IMG_SU3,
    RESOURCE_ID_IMG_SU4, RESOURCE_ID_IMG_SU5, RESOURCE_ID_IMG_SU6, RESOURCE_ID_IMG_SU7,
    RESOURCE_ID_IMG_SU8, RESOURCE_ID_IMG_SU9
};

static const uint32_t DATE_LOWERCASE_TENS_RESOURCES[] = {
    RESOURCE_ID_NONE, RESOURCE_ID_IMG_SL10, RESOURCE_ID_IMG_SL20, RESOURCE_ID_IMG_SL30,
};

static const uint32_t DATE_LOWERCASE_ONES_RESOURCES[] = {
    RESOURCE_ID_IMG_SL10, RESOURCE_ID_IMG_SL1, RESOURCE_ID_IMG_SL2, RESOURCE_ID_IMG_SL3,
    RESOURCE_ID_IMG_SL4, RESOURCE_ID_IMG_SL5, RESOURCE_ID_IMG_SL6, RESOURCE_ID_IMG_SL7,
    RESOURCE_ID_IMG_SL8, RESOURCE_ID_IMG_SL9,
};

// ==================== 主題系統 ====================

static void theme_resolve_colors(ThemeConfig *theme) {
#if defined(PBL_COLOR)
    // 彩色平台的顏色由 AppMessage 直接寫入，不需在此計算
#else
    theme->background = theme->is_dark ? GColorBlack : GColorWhite;
    theme->text = theme->is_dark ? GColorWhite : GColorBlack;
    
    // 小時強調色：設為背景色時特定區域變為挖空顯示，設為文字色時正常顯示。
    // 此設計讓使用者可選擇黑白表盤是否突顯小時數字。
    theme->hour_accent = theme->bw_hour_accent ? theme->background : theme->text;
    
    // 黑白平台不支援獨立的分鐘強調色，統一沿用文字色
    theme->minute_accent = theme->text;
#endif
}

static void theme_init_defaults(ThemeConfig *theme) {
#if defined(PBL_COLOR)
    theme->background = GColorBlack;
    theme->text = GColorWhite;
    theme->hour_accent = GColorChromeYellow;
    theme->minute_accent = GColorChromeYellow;
#else
    theme->is_dark = true;
    theme->bw_hour_accent = true;
    theme_resolve_colors(theme);
#endif
}

static void theme_load_from_storage(ThemeConfig *theme) {
    theme_init_defaults(theme);

#if defined(PBL_COLOR)
    if (persist_exists(KEY_BACKGROUND_COLOR)) {
        theme->background = GColorFromHEX(persist_read_int(KEY_BACKGROUND_COLOR));
    }
    if (persist_exists(KEY_TEXT_COLOR)) {
        theme->text = GColorFromHEX(persist_read_int(KEY_TEXT_COLOR));
    }
    if (persist_exists(KEY_HOUR_COLOR)) {
        theme->hour_accent = GColorFromHEX(persist_read_int(KEY_HOUR_COLOR));
    }
    if (persist_exists(KEY_MINUTE_COLOR)) {
        theme->minute_accent = GColorFromHEX(persist_read_int(KEY_MINUTE_COLOR));
    }
#else
    if (persist_exists(KEY_THEME_IS_DARK)) {
        theme->is_dark = persist_read_bool(KEY_THEME_IS_DARK);
    }
    if (persist_exists(KEY_BW_HOUR_ACCENT)) {
        theme->bw_hour_accent = persist_read_bool(KEY_BW_HOUR_ACCENT);
    }
    theme_resolve_colors(theme);
#endif
}

static int get_palette_size(GBitmap *bitmap) {
    switch (gbitmap_get_format(bitmap)) {
        case GBitmapFormat1Bit:
        case GBitmapFormat1BitPalette: return 2;
        case GBitmapFormat2BitPalette:  return 4;
        case GBitmapFormat4BitPalette:  return 16;
        default:                        return 0;  // GBitmapFormat8Bit 等無調色盤格式
    }
}

static void theme_apply_to_bitmap(const ThemeConfig *theme, GBitmap *bitmap, LayerType type) {
    if (!bitmap) return;

    GColor *palette = gbitmap_get_palette(bitmap);
    if (!palette) {
        // 8-bit 等非調色盤格式無法直接修改顏色，跳過主題套用
        return;
    }

    int palette_size = get_palette_size(bitmap);
    if (palette_size == 0) return;

    GColor accent_color = (type == LAYER_TYPE_HOUR) ? theme->hour_accent :
                          (type == LAYER_TYPE_MINUTE_ACCENT) ? theme->minute_accent :
                          theme->text;

    // 圖片資源以固定色作為語意插槽，此處將其替換為當前主題配色：
    //   彩色平台：Red → 強調色，Black → 文字色（或強調色，視圖層類型）
    //   黑白平台：White → 強調色（同背景色），Black → 文字色（單色調色盤的唯一前景色）
    for (int i = 0; i < palette_size; i++) {
#if defined(PBL_COLOR)
        if (gcolor_equal(palette[i], GColorRed)) {
            palette[i] = accent_color;
        } else if (gcolor_equal(palette[i], GColorBlack)) {
            palette[i] = (type == LAYER_TYPE_MINUTE_ACCENT) ? accent_color : theme->text;
        }
#else
        if (gcolor_equal(palette[i], GColorBlack)) {
            palette[i] = theme->text;
        } else if (gcolor_equal(palette[i], GColorWhite)) {
            palette[i] = accent_color;
        }
#endif
    }
}

// ==================== 圖層管理系統 ====================

static void display_layer_init(DisplayLayer *dl, Layer *parent, GRect frame, LayerType type) {
    if (!dl || !parent) return;

    memset(dl, 0, sizeof(DisplayLayer));

    dl->base_frame = frame;
    dl->type = type;
    dl->anim_state = ANIM_STATE_IDLE;

    dl->layer = bitmap_layer_create(frame);
    if (!dl->layer) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create bitmap layer");
        return;
    }

    // GCompOpSet：保留圖片的透明通道，確保多圖層疊加時背景透明正確顯示
    bitmap_layer_set_background_color(dl->layer, GColorClear);
    bitmap_layer_set_compositing_mode(dl->layer, GCompOpSet);
    layer_add_child(parent, bitmap_layer_get_layer(dl->layer));
}

static void display_layer_cleanup_animation(DisplayLayer *dl) {
    if (!dl) return;

    if (dl->animation) {
        animation_unschedule((Animation *)dl->animation);
        property_animation_destroy(dl->animation);
        dl->animation = NULL;
    }
    dl->anim_state = ANIM_STATE_IDLE;
}

static void display_layer_load_resource(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl) return;

    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
        dl->bitmap = NULL;
    }

    if (resource_id != RESOURCE_ID_NONE) {
        dl->bitmap = gbitmap_create_with_resource(resource_id);
        if (!dl->bitmap) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to load resource: %lu", resource_id);
            return;
        }
        theme_apply_to_bitmap(&s_app.theme, dl->bitmap, dl->type);
    }

    if (dl->layer) {
        bitmap_layer_set_bitmap(dl->layer, dl->bitmap);
    }
}

static void display_layer_set_position(DisplayLayer *dl, bool offset_for_animation) {
    if (!dl || !dl->layer) return;

    GRect frame = dl->base_frame;
    if (offset_for_animation) {
        frame.origin.y += ANIMATION_OFFSET_Y;
    }

    layer_set_frame(bitmap_layer_get_layer(dl->layer), frame);
}

static void display_layer_deinit(DisplayLayer *dl) {
    if (!dl) return;

    display_layer_cleanup_animation(dl);

    if (dl->bitmap) {
        gbitmap_destroy(dl->bitmap);
        dl->bitmap = NULL;
    }
    
    if (dl->layer) {
        bitmap_layer_destroy(dl->layer);
        dl->layer = NULL;
    }

    dl->current_resource_id = RESOURCE_ID_NONE;
    dl->anim_state = ANIM_STATE_IDLE;
}

// ==================== 圖層遍歷系統 ====================

typedef void (*LayerIteratorCallback)(DisplayLayer *dl, void *context);

static void iterate_all_layers(LayerIteratorCallback callback, void *context) {
    if (!callback) return;

    DisplayLayer *all_layers[] = {
        &s_app.hour_layers[0], &s_app.hour_layers[1],
        &s_app.minute_layers[0], &s_app.minute_layers[1],
        &s_app.month_layers[0], &s_app.month_layers[1],
        &s_app.day_layers[0], &s_app.day_layers[1],
        &s_app.week_layer, &s_app.yue_layer, &s_app.ri_layer, &s_app.zhou_layer
    };

    for (size_t i = 0; i < ARRAY_LENGTH(all_layers); i++) {
        if (all_layers[i]) {
            callback(all_layers[i], context);
        }
    }
}

static void iterate_animated_layers(LayerIteratorCallback callback, void *context) {
    if (!callback) return;

    DisplayLayer *animated_layers[] = {
        &s_app.hour_layers[0], &s_app.hour_layers[1],
        &s_app.minute_layers[0], &s_app.minute_layers[1],
        &s_app.month_layers[0], &s_app.month_layers[1],
        &s_app.day_layers[0], &s_app.day_layers[1],
        &s_app.week_layer
    };

    for (size_t i = 0; i < ARRAY_LENGTH(animated_layers); i++) {
        if (animated_layers[i]) {
            callback(animated_layers[i], context);
        }
    }
}

// ==================== 遍歷回調函數 (Callbacks) ====================

// 釋放圖層的所有資源（動畫、點陣圖、圖層物件）
static void teardown_layer_cb(DisplayLayer *dl, void *context) {
    display_layer_deinit(dl);
}

// 重新套用主題色至點陣圖調色盤，並標記圖層需重繪
static void refresh_theme_cb(DisplayLayer *dl, void *context) {
    if (dl->bitmap) {
        theme_apply_to_bitmap(&s_app.theme, dl->bitmap, dl->type);
        if (dl->layer) {
            layer_mark_dirty(bitmap_layer_get_layer(dl->layer));
        }
    }
}

// 依當前動畫設定調整圖層起始位置（啟用時下偏 ANIMATION_OFFSET_Y，關閉時歸位至基準位置）
static void set_anim_pos_cb(DisplayLayer *dl, void *context) {
    display_layer_cleanup_animation(dl);
    display_layer_set_position(dl, false);
}

// ==================== 動畫系統 ====================
//
// 換圖動畫採兩段式設計：
//   第一段（fade-out）：圖層下滑離場
//   第二段（fade-in） ：載入新圖後上滑入場，由 anim_fade_out_stopped 串接觸發
// 兩段動畫各佔 ANIMATION_DURATION_MS 的一半，分別使用 EaseIn 與 EaseOut 曲線。

static void anim_fade_in_stopped(Animation *anim, bool finished, void *context) {
    DisplayLayer *dl = (DisplayLayer *)context;
    if (!dl || !dl->layer) return;

    if (!finished) {
        // 若入場動畫被中斷（如分鐘快速連切），強制歸位至基準位置，避免圖層殘留偏移
        display_layer_set_position(dl, false);
    }

    display_layer_cleanup_animation(dl);
}

static void anim_fade_out_stopped(Animation *anim, bool finished, void *context) {
    DisplayLayer *dl = (DisplayLayer *)context;
    if (!dl || !dl->layer) {
        if (dl) display_layer_cleanup_animation(dl);
        return;
    }

    if (!finished) {
        // 若離場動畫被中斷，強制載入目標資源並歸位，避免顯示殘留的舊內容
        display_layer_load_resource(dl, dl->current_resource_id);
        display_layer_set_position(dl, false);
        display_layer_cleanup_animation(dl);
        return;
    }

    Layer *layer = bitmap_layer_get_layer(dl->layer);
    if (!layer) {
        display_layer_cleanup_animation(dl);
        return;
    }

    // 第二段：載入新資源後，從當前（已下滑）位置上滑回基準位置
    display_layer_load_resource(dl, dl->current_resource_id);

    GRect from = layer_get_frame(layer);
    GRect to = dl->base_frame;

    dl->animation = property_animation_create_layer_frame(layer, &from, &to);
    if (!dl->animation) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create fade-in animation, falling back to static update");
        display_layer_set_position(dl, false);
        display_layer_cleanup_animation(dl);
        return;
    }

    dl->anim_state = ANIM_STATE_FADE_IN;
    animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
    animation_set_curve((Animation *)dl->animation, AnimationCurveEaseOut);
    animation_set_handlers((Animation *)dl->animation,
                           (AnimationHandlers){.stopped = anim_fade_in_stopped}, dl);
    animation_schedule((Animation *)dl->animation);
}

static void display_layer_update_animated(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl || !dl->layer) return;

    display_layer_cleanup_animation(dl);

    if (dl->current_resource_id == RESOURCE_ID_NONE) {
        // 圖層尚無內容，動畫期間完全不可見，直接載入並定位即可
        display_layer_load_resource(dl, resource_id);
        dl->current_resource_id = resource_id;
        display_layer_set_position(dl, false);
        return;
    }

    Layer *layer = bitmap_layer_get_layer(dl->layer);
    GRect from = layer_get_frame(layer);
    dl->current_resource_id = resource_id;

    // 第一段：從當前位置下滑離場，結束後由 anim_fade_out_stopped 串接觸發入場動畫
    GRect to = from;
    to.origin.y += ANIMATION_OFFSET_Y;
    
    dl->animation = property_animation_create_layer_frame(layer, &from, &to);
    if (dl->animation) {
        dl->anim_state = ANIM_STATE_FADE_OUT;
        animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
        animation_set_curve((Animation *)dl->animation, AnimationCurveEaseIn);
        animation_set_handlers((Animation *)dl->animation,
                              (AnimationHandlers){.stopped = anim_fade_out_stopped}, dl);
        animation_schedule((Animation *)dl->animation);
    } else {
        // 動畫建立失敗時直接靜態更新，
        // 避免 current_resource_id 已更新但 bitmap 未載入導致圖層卡死
        APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to create fade-out animation, falling back to static update");
        display_layer_load_resource(dl, resource_id);
        display_layer_set_position(dl, false);
    }
}

static void display_layer_update_static(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl || !dl->layer) return;

    display_layer_cleanup_animation(dl);
    display_layer_set_position(dl, false);
    display_layer_load_resource(dl, resource_id);
    dl->current_resource_id = resource_id;
}

static void display_layer_update(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl || dl->current_resource_id == resource_id) return;

    if (s_app.animation_enabled) {
        display_layer_update_animated(dl, resource_id);
    } else {
        display_layer_update_static(dl, resource_id);
    }
}

// ==================== 日期區塊佈局與動畫（Emery 專用）====================
//
// Emery 平台上，月與日合併為一條字串「X月X日」，依實際出現的字形緊鄰排列
// （字與字間距 DATE_GLYPH_GAP）並於月日區域內水平置中。
// 顯示以「月／日／週」三個區塊為單位：當某區塊的數值在初次載入或日期變動時
// 發生改變，整塊（含數字與「月／日／周」標籤）一起播放跳動動畫。

#if defined(PBL_PLATFORM_EMERY)

// 強制讓圖層播放一次跳動動畫（下沉→載入新圖→上升回 base_frame），
// 不受「內容未變」或「先前為空」限制，使同一區塊內所有字能同步跳動。
static void date_layer_jump(DisplayLayer *dl, uint32_t resource_id) {
    if (!dl || !dl->layer) return;

    // 無內容的字形（如單位數月份的十位）直接清空歸位，毋須動畫
    if (resource_id == RESOURCE_ID_NONE) {
        display_layer_update_static(dl, RESOURCE_ID_NONE);
        return;
    }

    display_layer_cleanup_animation(dl);

    Layer *layer = bitmap_layer_get_layer(dl->layer);
    dl->current_resource_id = resource_id;

    // 第一段：自當前位置下沉，結束後由 anim_fade_out_stopped 載入新圖並上升回 base_frame
    GRect from = layer_get_frame(layer);
    GRect to = from;
    to.origin.y += ANIMATION_OFFSET_Y;

    dl->animation = property_animation_create_layer_frame(layer, &from, &to);
    if (dl->animation) {
        dl->anim_state = ANIM_STATE_FADE_OUT;
        animation_set_duration((Animation *)dl->animation, ANIMATION_DURATION_MS / 2);
        animation_set_curve((Animation *)dl->animation, AnimationCurveEaseIn);
        animation_set_handlers((Animation *)dl->animation,
                              (AnimationHandlers){.stopped = anim_fade_out_stopped}, dl);
        animation_schedule((Animation *)dl->animation);
    } else {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Failed to create date jump animation, falling back to static update");
        display_layer_load_resource(dl, resource_id);
        display_layer_set_position(dl, false);
    }
}

// 區塊未變動時，僅將圖層歸位至（可能因重新置中而位移的）base_frame，內容保持不變。
static void date_layer_reposition(DisplayLayer *dl) {
    if (!dl) return;
    display_layer_cleanup_animation(dl);
    display_layer_set_position(dl, false);
}

// 設定日期圖層於日期列上的水平位置（更新 base_frame）。
static void date_layer_set_base(DisplayLayer *dl, int x) {
    if (!dl) return;
    dl->base_frame = GRect(x, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h);
}

// 套用單一區塊：changed 為真時整塊跳動（動畫關閉時改為靜態更新），
// 否則僅重新歸位（內容不變，因應月日字串重新置中所造成的位移）。
static void date_block_apply(DisplayLayer **layers, const uint32_t *res, int count, bool changed) {
    for (int i = 0; i < count; i++) {
        if (changed) {
            if (s_app.animation_enabled) {
                date_layer_jump(layers[i], res[i]);
            } else {
                display_layer_update_static(layers[i], res[i]);
            }
        } else {
            date_layer_reposition(layers[i]);
        }
    }
}

static void layout_and_animate_date_emery(int month, int day, int week,
        uint32_t month_tens, uint32_t month_ones,
        uint32_t day_tens, uint32_t day_ones, uint32_t week_res) {

    const int step = DATE_IMAGE_SIZE.w + DATE_GLYPH_GAP;

    // 月日字串的字形順序：[月十位][月個位][月][日十位][日個位][日]
    // 缺省（NONE）的字形不佔位，使整條字串始終緊鄰排列。
    DisplayLayer *md_layers[6] = {
        &s_app.month_layers[0], &s_app.month_layers[1], &s_app.yue_layer,
        &s_app.day_layers[0], &s_app.day_layers[1], &s_app.ri_layer
    };
    uint32_t md_res[6] = {
        month_tens, month_ones, RESOURCE_ID_IMG_YUE,
        day_tens, day_ones, RESOURCE_ID_IMG_RI
    };

    int present = 0;
    for (int i = 0; i < 6; i++) {
        if (md_res[i] != RESOURCE_ID_NONE) present++;
    }

    // 字串總寬 = 各字寬度相加 + 字間 1px 間距，於區域內置中求得起始 x
    int total_w = present * DATE_IMAGE_SIZE.w + (present - 1) * DATE_GLYPH_GAP;
    int start_x = DATE_MD_REGION_X + (DATE_MD_REGION_W - total_w) / 2;

    int cursor = start_x;
    for (int i = 0; i < 6; i++) {
        if (md_res[i] != RESOURCE_ID_NONE) {
            date_layer_set_base(md_layers[i], cursor);
            cursor += step;
        } else {
            // 缺省字形稍後會被清空隱藏，位置無妨
            date_layer_set_base(md_layers[i], start_x);
        }
    }

    // 週區塊位置固定於右側
    date_layer_set_base(&s_app.zhou_layer, DATE_ZHOU_X);
    date_layer_set_base(&s_app.week_layer, DATE_WEEK_X);

    bool month_changed = (month != s_app.last_month);
    bool day_changed   = (day   != s_app.last_day);
    bool week_changed  = (week  != s_app.last_week);

    // 月區塊：十位、個位、「月」
    DisplayLayer *month_block[3] = { &s_app.month_layers[0], &s_app.month_layers[1], &s_app.yue_layer };
    uint32_t month_block_res[3]  = { month_tens, month_ones, RESOURCE_ID_IMG_YUE };
    date_block_apply(month_block, month_block_res, 3, month_changed);

    // 日區塊：十位、個位、「日」
    DisplayLayer *day_block[3] = { &s_app.day_layers[0], &s_app.day_layers[1], &s_app.ri_layer };
    uint32_t day_block_res[3]  = { day_tens, day_ones, RESOURCE_ID_IMG_RI };
    date_block_apply(day_block, day_block_res, 3, day_changed);

    // 週區塊：「周」、週值
    DisplayLayer *week_block[2] = { &s_app.zhou_layer, &s_app.week_layer };
    uint32_t week_block_res[2]  = { RESOURCE_ID_IMG_ZHOU, week_res };
    date_block_apply(week_block, week_block_res, 2, week_changed);

    s_app.last_month = month;
    s_app.last_day = day;
    s_app.last_week = week;
}

#endif // PBL_PLATFORM_EMERY

// ==================== 時間更新邏輯 ====================

static void update_time_display(struct tm *tick_time) {
    if (!tick_time) return;

    int hour = tick_time->tm_hour;
    if (!clock_is_24h_style()) {
        hour = hour % 12;
        if (hour == 0) hour = 12;
    }

    uint32_t hour_tens = (hour == 10) ? 
    RESOURCE_ID_NONE : 
    TIME_UPPERCASE_TENS_RESOURCES[hour / 10];
    uint32_t hour_ones = (hour == 0) ? 
    RESOURCE_ID_IMG_U0 : 
    TIME_UPPERCASE_ONES_RESOURCES[hour % 10];

    display_layer_update(&s_app.hour_layers[0], hour_tens);
    display_layer_update(&s_app.hour_layers[1], hour_ones);

    int minute = tick_time->tm_min;
    uint32_t minute_tens = RESOURCE_ID_NONE;
    uint32_t minute_ones = RESOURCE_ID_NONE;

    // 中文時間慣用語的特殊對應：
    //   :00 → 「點整」（如「三點整」），:30 → 「點半」（如「三點半」）
    //   :10 → 使用 L1 + L0 組合，因為「10分」在中文口語中通常念「十分」
    // 其餘分鐘：個位為 0 時（如 :20）十位圖本身即含「十」字，個位圖留空
    if (minute == 0) {
        minute_tens = RESOURCE_ID_IMG_DIAN;
        minute_ones = RESOURCE_ID_IMG_ZHENG;
    } else if (minute == 30) {
        minute_tens = RESOURCE_ID_IMG_DIAN;
        minute_ones = RESOURCE_ID_IMG_BAN;
    } else if (minute == 10) {
        minute_tens = RESOURCE_ID_IMG_L1;
        minute_ones = RESOURCE_ID_IMG_L0;
    } else {
        int m1 = minute / 10;
        int m2 = minute % 10;
        minute_tens = (m2 == 0) ? TIME_LOWERCASE_ONES_RESOURCES[m1] :
                      TIME_LOWERCASE_TENS_RESOURCES[m1];
        minute_ones = TIME_LOWERCASE_ONES_RESOURCES[m2];
    }

    display_layer_update(&s_app.minute_layers[0], minute_tens);
    display_layer_update(&s_app.minute_layers[1], minute_ones);
}

static void update_date_display(struct tm *tick_time) {
    if (!tick_time) return;

    int month = tick_time->tm_mon + 1;
    int day = tick_time->tm_mday;
    int week = tick_time->tm_wday;

    uint32_t month_tens = (month > 10) ? RESOURCE_ID_IMG_SU10 : RESOURCE_ID_NONE;
    uint32_t month_ones = DATE_UPPERCASE_ONES_RESOURCES[month % 10];

    int d1 = day / 10;
    int d2 = day % 10;
    uint32_t day_tens = RESOURCE_ID_NONE;
    uint32_t day_ones = RESOURCE_ID_NONE;

    day_ones = DATE_LOWERCASE_ONES_RESOURCES[d2];

    if (day > 10) {
        if (d2 == 0) {
            day_tens = DATE_LOWERCASE_ONES_RESOURCES[d1];
        } else {
            day_tens = DATE_LOWERCASE_TENS_RESOURCES[d1];
        }
    }

    uint32_t week_res = (week == 0) ? RESOURCE_ID_IMG_RI :
                        DATE_LOWERCASE_ONES_RESOURCES[week];

#if defined(PBL_PLATFORM_EMERY)
    // Emery：月日合併為置中字串，並以區塊為單位播放跳動動畫
    layout_and_animate_date_emery(month, day, week,
                                  month_tens, month_ones,
                                  day_tens, day_ones, week_res);
#else
    display_layer_update(&s_app.month_layers[0], month_tens);
    display_layer_update(&s_app.month_layers[1], month_ones);
    display_layer_update(&s_app.day_layers[0], day_tens);
    display_layer_update(&s_app.day_layers[1], day_ones);
    display_layer_update(&s_app.week_layer, week_res);
#endif
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time_display(tick_time);
    if (units_changed & DAY_UNIT) {
        update_date_display(tick_time);
    }
}

// ==================== UI 構建 ====================

static void setup_all_layers(Layer *parent) {
    DisplayLayer *layers[] = {
        &s_app.hour_layers[0], &s_app.hour_layers[1],
        &s_app.minute_layers[0], &s_app.minute_layers[1],
        &s_app.month_layers[0], &s_app.month_layers[1],
        &s_app.day_layers[0], &s_app.day_layers[1],
        &s_app.week_layer, &s_app.yue_layer, &s_app.ri_layer, &s_app.zhou_layer
    };

    GRect frames[] = {
        GRect(TIME_COL1_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(TIME_COL2_X, TIME_ROW1_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(TIME_COL1_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(TIME_COL2_X, TIME_ROW2_Y, TIME_IMAGE_SIZE.w, TIME_IMAGE_SIZE.h),
        GRect(DATE_MONTH1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_MONTH2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_DAY1_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_DAY2_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_WEEK_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_YUE_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_RI_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
        GRect(DATE_ZHOU_X, DATE_ROW_Y, DATE_IMAGE_SIZE.w, DATE_IMAGE_SIZE.h),
    };

    LayerType types[] = {
        LAYER_TYPE_HOUR, LAYER_TYPE_HOUR,
        LAYER_TYPE_MINUTE_NORMAL, LAYER_TYPE_MINUTE_ACCENT,
        LAYER_TYPE_DATE, LAYER_TYPE_DATE, LAYER_TYPE_DATE, LAYER_TYPE_DATE,
        LAYER_TYPE_DATE, LAYER_TYPE_STATIC, LAYER_TYPE_STATIC, LAYER_TYPE_STATIC
    };

    // 靜態資源（月、日、周）在初始化時一次性載入，不隨時間更新；
    // 動態圖層（時、分、日期數字）初始為 NONE，由 update_time_display / update_date_display 填入
    uint32_t static_resources[] = {
        RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE,
        RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE, RESOURCE_ID_NONE,
        RESOURCE_ID_NONE, RESOURCE_ID_IMG_YUE, RESOURCE_ID_IMG_RI, RESOURCE_ID_IMG_ZHOU
    };

    for (size_t i = 0; i < ARRAY_LENGTH(layers); i++) {
        display_layer_init(layers[i], parent, frames[i], types[i]);
        display_layer_set_position(layers[i], s_app.animation_enabled && types[i] != LAYER_TYPE_STATIC);

        if (static_resources[i] != RESOURCE_ID_NONE) {
            display_layer_load_resource(layers[i], static_resources[i]);
            layers[i]->current_resource_id = static_resources[i];
        }
    }
}

static void teardown_all_layers(void) {
    iterate_all_layers(teardown_layer_cb, NULL);
}

static void refresh_all_layer_themes(void) {
    iterate_all_layers(refresh_theme_cb, NULL);
}

static void apply_theme_to_window(void) {
    window_set_background_color(s_app.main_window, s_app.theme.background);
    refresh_all_layer_themes();
}

static void main_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    setup_all_layers(window_layer);

    time_t now = time(NULL);
    struct tm *current_time = localtime(&now);
    update_time_display(current_time);
    update_date_display(current_time);
}

static void main_window_unload(Window *window) {
    teardown_all_layers();
}

// ==================== AppMessage 處理 ====================

static void handle_settings_update(DictionaryIterator *iter) {
    if (!iter) return;
    
    bool theme_changed = false;

    // 步驟一：讀取並套用各項設定
#if defined(PBL_COLOR)
    Tuple *bg = dict_find(iter, KEY_BACKGROUND_COLOR);
    if (bg) {
        s_app.theme.background = GColorFromHEX(bg->value->int32);
        persist_write_int(KEY_BACKGROUND_COLOR, bg->value->int32);
        theme_changed = true;
    }

    Tuple *text = dict_find(iter, KEY_TEXT_COLOR);
    if (text) {
        s_app.theme.text = GColorFromHEX(text->value->int32);
        persist_write_int(KEY_TEXT_COLOR, text->value->int32);
        theme_changed = true;
    }
#else
    Tuple *dark = dict_find(iter, KEY_THEME_IS_DARK);
    if (dark) {
        s_app.theme.is_dark = dark->value->int32 == 1;
        persist_write_bool(KEY_THEME_IS_DARK, s_app.theme.is_dark);
        theme_changed = true;
    }

    Tuple *hour_bg = dict_find(iter, KEY_BW_HOUR_ACCENT);
    if (hour_bg) {
        s_app.theme.bw_hour_accent = hour_bg->value->int32 == 1;
        persist_write_bool(KEY_BW_HOUR_ACCENT, s_app.theme.bw_hour_accent);
        theme_changed = true;
    }
#endif

    Tuple *minute_color = dict_find(iter, KEY_MINUTE_COLOR);
    if (minute_color) {
#if defined(PBL_COLOR)
        // 黑白平台的強調色由 theme_resolve_colors() 強制計算，無需寫入 flash
        s_app.theme.minute_accent = GColorFromHEX(minute_color->value->int32);
        persist_write_int(KEY_MINUTE_COLOR, minute_color->value->int32);
#endif
        theme_changed = true;
    }

    Tuple *hour_color = dict_find(iter, KEY_HOUR_COLOR);
    if (hour_color) {
#if defined(PBL_COLOR)
        s_app.theme.hour_accent = GColorFromHEX(hour_color->value->int32);
        persist_write_int(KEY_HOUR_COLOR, hour_color->value->int32);
#endif
        theme_changed = true;
    }

    // 步驟二：重新計算衍生色，確保黑白平台的色彩約束覆蓋原始輸入
    theme_resolve_colors(&s_app.theme);

    // 步驟三：套用主題至視窗背景與所有圖層
    if (theme_changed) {
        apply_theme_to_window();
    }

    // 步驟四：套用動畫設定，靜態圖層（月、日、周）位置固定，不隨此設定變動
    Tuple *anim = dict_find(iter, KEY_ANIMATION_ENABLED);
    if (anim) {
        s_app.animation_enabled = anim->value->int32 == 1;
        persist_write_bool(KEY_ANIMATION_ENABLED, s_app.animation_enabled);

        iterate_animated_layers(set_anim_pos_cb, NULL);
    }
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    handle_settings_update(iter);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

static void outbox_failed_handler(DictionaryIterator *iter, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
}

// ==================== 應用程式生命週期 ====================

static void app_init(void) {
    memset(&s_app, 0, sizeof(AppState));

    // 以 -1 標記「尚未顯示過」，確保初次載入時月／日／週三區塊都會播放跳動動畫
    s_app.last_month = -1;
    s_app.last_day = -1;
    s_app.last_week = -1;

    theme_load_from_storage(&s_app.theme);
    
    if (persist_exists(KEY_ANIMATION_ENABLED)) {
        s_app.animation_enabled = persist_read_bool(KEY_ANIMATION_ENABLED);
    } else {
        s_app.animation_enabled = true;
    }

    s_app.main_window = window_create();
    if (!s_app.main_window) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create window");
        return;
    }
    
    window_set_background_color(s_app.main_window, s_app.theme.background);
    window_set_window_handlers(s_app.main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload,
    });

    window_stack_push(s_app.main_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_register_outbox_failed(outbox_failed_handler);
    
    // inbox/outbox 各 128 bytes，足以容納所有設定鍵值
    AppMessageResult result = app_message_open(128, 128);
    if (result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage open failed: %d", (int)result);
    }
}

static void app_deinit(void) {
    tick_timer_service_unsubscribe();
    app_message_deregister_callbacks();
    
    if (s_app.main_window) {
        window_destroy(s_app.main_window);
    }
}

int main(void) {
    app_init();
    app_event_loop();
    app_deinit();
}
