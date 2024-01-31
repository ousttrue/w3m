import { themes as prismThemes } from "prism-react-renderer";
import type { Config } from "@docusaurus/types";
import type * as Preset from "@docusaurus/preset-classic";
import remarkDefList from "remark-deflist";

const title = "w3mノート";

const config: Config = {
  title,
  tagline: "w3m custom",
  favicon: "img/favicon.ico",

  // gh-pages
  url: "https://ousttrue.github.io",
  baseUrl: "/w3m/",

  // GitHub pages deployment config.
  // If you aren't using GitHub pages, you don't need these.
  organizationName: "ousttrue", // Usually your GitHub org/user name.
  projectName: "w3m", // Usually your repo name.

  onBrokenLinks: "throw",
  onBrokenMarkdownLinks: "warn",

  // Even if you don't use internationalization, you can use this field to set
  // useful metadata like html lang. For example, if your site is Chinese, you
  // may want to replace "en" with "zh-Hans".
  i18n: {
    defaultLocale: "ja",
    locales: ["ja"],
  },

  presets: [
    [
      "classic",
      {
        docs: {
          sidebarPath: "./sidebars.ts",
          remarkPlugins: [
            remarkDefList,
          ],
        },
        blog: {
          showReadingTime: false,
          blogSidebarCount: "ALL",
          postsPerPage: "ALL",
          sortPosts: "descending",
        },
        theme: {
          customCss: "./src/css/custom.css",
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    // Replace with your project's social card
    navbar: {
      title,
      items: [
        {
          type: "docSidebar",
          sidebarId: "tutorialSidebar",
          position: "left",
          label: "doc-jp",
        },
        { to: "/blog", label: "note", position: "left" },
        //
        {
          href: "https://docusaurus.io/docs",
          label: "docusaurus-docs",
          position: "right",
        },
        {
          href: "https://github.com/ousttrue/w3m",
          label: "GitHub",
          position: "right",
        },
      ],
    },
    footer: {
      style: "dark",
      copyright: `Copyright © ${
        new Date().getFullYear()
      } ${title}, Inc. Built with Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
