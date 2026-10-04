/**
 * Ghost Continuum landing i18n (client packs under /i18n/{lang}.json)
 * Locale: ?lang=xx | localStorage groklink_lang | navigator
 */
(function () {
  var STORAGE = "groklink_lang";
  var DEFAULT = "en";
  var SUPPORTED = ["en"];

  function normalize(code) {
    if (!code) return DEFAULT;
    var c = String(code).replace("_", "-");
    if (SUPPORTED.indexOf(c) >= 0) return c;
    var base = c.split("-")[0];
    if (base === "zh") return c.toLowerCase().indexOf("tw") >= 0 || c.toLowerCase().indexOf("hant") >= 0 ? "zh-TW" : "zh-CN";
    if (base === "pt") return "pt-BR";
    if (SUPPORTED.indexOf(base) >= 0) return base;
    return DEFAULT;
  }

  function detect() {
    return DEFAULT;
  }

  /** Support flat "hero.title" or nested { hero: { title } } packs */
  function lookup(dict, key) {
    if (!dict || !key) return null;
    if (Object.prototype.hasOwnProperty.call(dict, key) && typeof dict[key] === "string") {
      return dict[key];
    }
    var parts = key.split(".");
    var cur = dict;
    for (var i = 0; i < parts.length; i++) {
      if (cur == null || typeof cur !== "object") return null;
      cur = cur[parts[i]];
    }
    return typeof cur === "string" ? cur : null;
  }

  function applyDict(dict) {
    if (!dict) return;
    document.querySelectorAll("[data-i18n]").forEach(function (el) {
      var key = el.getAttribute("data-i18n");
      var val = lookup(dict, key);
      if (val == null) return;
      if (el.tagName === "META") {
        el.setAttribute("content", val);
      } else if (el.tagName === "TITLE") {
        el.textContent = val;
      } else if (el.hasAttribute("data-i18n-html")) {
        el.innerHTML = val;
      } else {
        el.textContent = val;
      }
    });
    document.querySelectorAll("[data-i18n-attr]").forEach(function (el) {
      var spec = el.getAttribute("data-i18n-attr"); // attr:key
      if (!spec) return;
      var parts = spec.split(":");
      if (parts.length < 2) return;
      var attr = parts[0];
      var key = parts.slice(1).join(":");
      var val = lookup(dict, key);
      if (val != null) el.setAttribute(attr, val);
    });
    var title = lookup(dict, "meta.title");
    if (title) document.title = title;
    var lang = document.documentElement.lang || "";
    var rtl = ["ar", "he", "fa"].indexOf(lang) >= 0 || lang.indexOf("ar") === 0;
    document.documentElement.dir = rtl ? "rtl" : "ltr";
  }

  function load(lang) {
    lang = normalize(lang);
    document.documentElement.lang = lang === "pt-BR" ? "pt-BR" : lang.split("-")[0];
    if (lang === "zh-CN") document.documentElement.lang = "zh-CN";
    if (lang === "zh-TW") document.documentElement.lang = "zh-TW";
    try {
      localStorage.setItem(STORAGE, lang);
    } catch (e) {}
    var url = "/i18n/" + encodeURIComponent(lang) + ".json?v=1";
    return fetch(url)
      .then(function (r) {
        if (!r.ok) throw new Error("missing pack");
        return r.json();
      })
      .then(applyDict)
      .catch(function () {
        if (lang !== DEFAULT) return load(DEFAULT);
      });
  }

  function wireSwitcher(lang) {
    /* EN-only 2026-08-11: no language picker */
    try {
      ["langSelect","ghost-lang","netforge-lang","bailey-lang"].forEach(function (id) {
        var el = document.getElementById(id);
        if (!el) return;
        var wrap = el.closest(".lang-wrap, .lang-select-wrap, label, .site-controls") || el;
        if (wrap && wrap.style) wrap.style.display = "none";
      });
      document.querySelectorAll(".lang-select, select[data-lang], [data-lang-select]").forEach(function (el) {
        var wrap = el.closest(".lang-wrap, label") || el;
        if (wrap && wrap.style) wrap.style.display = "none";
      });
    } catch (e) {}
  }

  var lang = detect();
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", function () {
      wireSwitcher(lang);
      load(lang);
    });
  } else {
    wireSwitcher(lang);
    load(lang);
  }

  window.GrokLinkI18n = { load: load, detect: detect, SUPPORTED: SUPPORTED };
})();
