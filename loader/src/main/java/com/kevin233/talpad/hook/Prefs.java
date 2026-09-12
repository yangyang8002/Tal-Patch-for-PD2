package com.kevin233.talpad.hook;

import java.util.Collections;
import java.util.Map;
import java.util.TreeMap;

/**
 * 只读配置快照，接口形态与 SharedPreferences 子集保持一致，
 * 供 Hook 代码以与旧版 RemotePreferences 相同的方式读取配置。
 *
 * @author Kevin233 (https://github.com/Kevin233B)
 * @author yangyang8002 (https://github.com/yangyang8002)
 */
public final class Prefs {
    private final Map<String, Object> map;

    Prefs(Map<String, Object> map) {
        this.map = map == null ? Collections.emptyMap() : map;
    }

    public boolean contains(String key) {
        return map.containsKey(key);
    }

    public boolean getBoolean(String key, boolean defValue) {
        Object v = map.get(key);
        if (v instanceof Boolean) {
            return (Boolean) v;
        }
        if (v instanceof String) {
            return Boolean.parseBoolean((String) v);
        }
        return defValue;
    }

    public String getString(String key, String defValue) {
        Object v = map.get(key);
        return v instanceof String ? (String) v : defValue;
    }

    public int getInt(String key, int defValue) {
        Object v = map.get(key);
        if (v instanceof Number) {
            return ((Number) v).intValue();
        }
        if (v instanceof String) {
            try {
                return Integer.parseInt(((String) v).trim());
            } catch (Throwable ignored) {
            }
        }
        return defValue;
    }

    public long getLong(String key, long defValue) {
        Object v = map.get(key);
        if (v instanceof Number) {
            return ((Number) v).longValue();
        }
        if (v instanceof String) {
            try {
                return Long.parseLong(((String) v).trim());
            } catch (Throwable ignored) {
            }
        }
        return defValue;
    }

    public Map<String, ?> getAll() {
        return new TreeMap<>(map);
    }
}
