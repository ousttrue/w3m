import type { SidebarsConfig } from "@docusaurus/plugin-content-docs";

const sidebars: SidebarsConfig = {
  // But you can create a sidebar manually
  sidebar: [
    {
      type: "category",
      label: "doc-jp",
      link: {
        type: "doc",
        id: "doc-jp/markdown",
      },
      items: [
        "doc-jp/README",
        "doc-jp/STORY",
        "doc-jp/FAQ",
        "doc-jp/README.SSL",
        "doc-jp/README.cookie",
        "doc-jp/README.cygwin",
        "doc-jp/README.dict",
        "doc-jp/README.img",
        "doc-jp/README.keymap",
        "doc-jp/README.m17n",
        "doc-jp/README.mailcap",
        "doc-jp/README.menu",
        "doc-jp/README.migemo",
        "doc-jp/README.mouse",
        "doc-jp/README.passwd",
        "doc-jp/README.pre_form",
        "doc-jp/README.siteconf",
        "doc-jp/README.tab",
      ],
    },
    {
      type: "category",
      label: "bonus",
      link: {
        type: "doc",
        id: "Bonus/markdown",
      },
      items: [
        "Bonus/README",
      ],
    },
  ],
};

export default sidebars;
