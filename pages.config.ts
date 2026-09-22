import { definePages } from "./vendor/pages/src/index.ts";

export default definePages({
  source: "web",
  out: "site",
  pages: [
    { from: "index.html", route: "/" },
    { from: "level-builder.html", route: "/level-builder/" },
    { from: "world-editor.html", route: "/world-editor/" }
  ],
  css: {
    files: [
      "tokens.css",
      "base.css",
      "type.css",
      "util.css",
      "responsive.css"
    ]
  },
  runtime: {
    base: "/isoweb/"
  }
});
