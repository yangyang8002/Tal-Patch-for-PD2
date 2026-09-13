package com.kevin233.talpad.hook;
/**
 * 配置键与默认值（与 module/config.default.json 及 WebUI 保持一致）。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class Config {
    public static final String PREFS = "tal_patch_config";
    public static final String REMOTE_FILE = "tal_patch_config.json";
    public static final String KEY_RESTORE_NOTIFICATION = "restore_notification";
    public static final String KEY_UNLOCK_INSTALL = "unlock_install";
    public static final String KEY_BLOCK_UC_ENV_DETECT = "block_usercenter_detect";
    public static final String KEY_BLOCK_DEFAULT_WALLPAPER = "block_default_wallpaper";
    public static final String KEY_BLOCK_DEFAULT_LAUNCHER = "block_default_launcher";
    public static final String KEY_CUSTOM_USAGE_ENABLED = "custom_usage_enabled";
    public static final String KEY_STUDY_ONCE_PER_DAY = "study_once_per_day";
    public static final String KEY_CUSTOM_STUDY_MINUTES = "custom_study_minutes";
    public static final String KEY_CUSTOM_APP_USAGE = "custom_app_usage";
    public static final String KEY_BLOCK_ROOT_LOGS = "block_root_logs";
    public static final String KEY_BLOCK_STUDY_REPORT = "block_study_report";
    public static final String KEY_BLOCK_MINOR_CONTROL = "block_minor_control";
    public static final String KEY_UI_COLOR_MODE = "ui_color_mode";
    public static final String KEY_UI_CUSTOM_COLOR = "ui_custom_color";
    /** 附加注入包名（逗号分隔），对应 WebUI 的「附加作用域」。 */
    /** 全局默认：对所有应用恢复通知内容（应用粒度开关的默认值） */
    public static final String KEY_NOTIFY_ALL_ENABLED = "notify_all_enabled";
    /** 应用粒度通知覆盖表：JSON 字符串，如 {"com.xxx":false} */
    public static final String KEY_NOTIFY_APP_OVERRIDES = "notify_app_overrides";
    public static final String KEY_SCOPE_EXTRA = "scope_extra";
    /** WebUI 主题：miuix / material（仅 WebUI 使用，Hook 侧不读）。 */
    public static final String KEY_UI_THEME = "ui_theme";
    public static final boolean DEFAULT_RESTORE_NOTIFICATION = true;
    public static final boolean DEFAULT_UNLOCK_INSTALL = true;
    public static final boolean DEFAULT_BLOCK_UC_ENV_DETECT = true;
    public static final boolean DEFAULT_BLOCK_DEFAULT_WALLPAPER = false;
    public static final boolean DEFAULT_BLOCK_DEFAULT_LAUNCHER = false;
    public static final boolean DEFAULT_CUSTOM_USAGE_ENABLED = false;
    public static final boolean DEFAULT_STUDY_ONCE_PER_DAY = true;
    public static final String DEFAULT_CUSTOM_STUDY_MINUTES = "120";
    public static final String DEFAULT_CUSTOM_APP_USAGE = "";
    public static final boolean DEFAULT_BLOCK_ROOT_LOGS = true;
    public static final boolean DEFAULT_BLOCK_STUDY_REPORT = true;
    public static final boolean DEFAULT_BLOCK_MINOR_CONTROL = true;
    public static final String DEFAULT_UI_COLOR_MODE = "dynamic";
    public static final String DEFAULT_UI_CUSTOM_COLOR = "#6750A4";
    public static final boolean DEFAULT_NOTIFY_ALL_ENABLED = true;
    public static final String DEFAULT_NOTIFY_APP_OVERRIDES = "{}";
    public static final String DEFAULT_SCOPE_EXTRA = "";
    public static final String DEFAULT_UI_THEME = "miuix";
    private Config() {
    }
}
