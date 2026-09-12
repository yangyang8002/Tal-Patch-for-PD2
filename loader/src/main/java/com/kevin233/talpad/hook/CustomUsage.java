package com.kevin233.talpad.hook;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * 自定义学习时长 / 应用使用时长的配置解析。
 * 配置统一来自 {@link ConfigBridge}（Zygisk 注入时传入 + /sdcard 镜像热更）。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class CustomUsage {
    public static final String KEY_TODAY_USE_TIME = "today_use_time_duration_key";
    public static final long MAX_DAILY_MS = 24L * 60L * 60L * 1000L;
    public static final long MAX_DAILY_MINUTES = 24L * 60L;

    private CustomUsage() {
    }

    public static boolean isEnabled(Prefs prefs) {
        return prefs != null && prefs.getBoolean(
                Config.KEY_CUSTOM_USAGE_ENABLED,
                Config.DEFAULT_CUSTOM_USAGE_ENABLED);
    }

    public static boolean blockRootLogs(Prefs prefs) {
        return prefs == null || prefs.getBoolean(
                Config.KEY_BLOCK_ROOT_LOGS,
                Config.DEFAULT_BLOCK_ROOT_LOGS);
    }

    public static boolean blockUserCenterDetect(Prefs prefs) {
        return prefs == null || prefs.getBoolean(
                Config.KEY_BLOCK_UC_ENV_DETECT,
                Config.DEFAULT_BLOCK_UC_ENV_DETECT);
    }

    public static boolean blockStudyReport(Prefs prefs) {
        return prefs == null || prefs.getBoolean(
                Config.KEY_BLOCK_STUDY_REPORT,
                Config.DEFAULT_BLOCK_STUDY_REPORT);
    }

    public static boolean blockMinorControl(Prefs prefs) {
        return prefs == null || prefs.getBoolean(
                Config.KEY_BLOCK_MINOR_CONTROL,
                Config.DEFAULT_BLOCK_MINOR_CONTROL);
    }

    public static long studyMillis(Prefs prefs) {
        long minutes = Long.parseLong(Config.DEFAULT_CUSTOM_STUDY_MINUTES);
        if (prefs != null) {
            try {
                String raw = prefs.getString(
                        Config.KEY_CUSTOM_STUDY_MINUTES,
                        Config.DEFAULT_CUSTOM_STUDY_MINUTES);
                minutes = Long.parseLong(raw == null ? "" : raw.trim());
            } catch (Throwable ignored) {
            }
        }
        if (minutes < 0) {
            minutes = 0;
        }
        if (minutes > MAX_DAILY_MINUTES) {
            minutes = MAX_DAILY_MINUTES;
        }
        return minutes * 60000L;
    }

    public static Long appUsageMillis(Prefs prefs, String pkg) {
        if (pkg == null) {
            return null;
        }
        return appUsageEntries(prefs).get(pkg);
    }

    public static Map<String, Long> appUsageEntries(Prefs prefs) {
        Map<String, Long> result = new LinkedHashMap<>();
        String raw = prefs == null ? null
                : prefs.getString(Config.KEY_CUSTOM_APP_USAGE,
                Config.DEFAULT_CUSTOM_APP_USAGE);
        if (raw == null) {
            return result;
        }
        for (String line : raw.split("\n")) {
            line = line.trim();
            if (line.isEmpty()) {
                continue;
            }
            String name;
            String number;
            int colon = line.indexOf(':');
            int fullColon = line.indexOf('：');
            if (colon >= 0 && (fullColon < 0 || colon < fullColon)) {
                name = line.substring(0, colon).trim();
                number = line.substring(colon + 1).trim();
            } else if (fullColon >= 0) {
                name = line.substring(0, fullColon).trim();
                number = line.substring(fullColon + 1).trim();
            } else {
                String[] parts = line.split("\s+");
                if (parts.length < 2) {
                    continue;
                }
                name = parts[0].trim();
                number = parts[1].trim();
            }
            if (name.isEmpty()) {
                continue;
            }
            try {
                long minutes = Long.parseLong(number);
                if (minutes < 0) {
                    minutes = 0;
                }
                if (minutes > MAX_DAILY_MINUTES) {
                    minutes = MAX_DAILY_MINUTES;
                }
                result.put(name, minutes * 60000L);
            } catch (Throwable ignored) {
            }
        }
        return result;
    }
}
