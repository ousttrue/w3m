/* argument search 1 */
for (i = 1; i < argc; i++) {
  if (*argv[i] == '-') {
    if (!strcmp("-config", argv[i])) {
      argv[i] = "-dummy";
      if (++i >= argc)
        usage();
      config_file = argv[i];
      argv[i] = "-dummy";
    } else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
      help();
    else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i])) {
      fversion(stdout);
      exit(0);
    }
  }
}

/* argument search 2 */
i = 1;
while (i < argc) {
  if (*argv[i] == '-') {
    if (!strcmp("-t", argv[i])) {
      if (++i >= argc)
        usage();
      if (atoi(argv[i]) > 0)
        Tabstop = atoi(argv[i]);
    } else if (!strcmp("-r", argv[i]))
      ShowEffect = false;
    else if (!strcmp("-l", argv[i])) {
      if (++i >= argc)
        usage();
      if (atoi(argv[i]) > 0)
        PagerMax = atoi(argv[i]);
    } else if (!strcmp("-graph", argv[i]))
      UseGraphicChar = GRAPHIC_CHAR_DEC;
    else if (!strcmp("-no-graph", argv[i]))
      UseGraphicChar = GRAPHIC_CHAR_ASCII;
    else if (!strcmp("-T", argv[i])) {
      if (++i >= argc)
        usage();
      DefaultType = default_type = argv[i];
    } else if (!strcmp("-m", argv[i]))
      SearchHeader = search_header = true;
    else if (!strcmp("-v", argv[i]))
      visual_start = true;
    else if (!strcmp("-N", argv[i]))
      open_new_tab = true;
    else if (!strcmp("-B", argv[i]))
      load_bookmark = true;
    else if (!strcmp("-bookmark", argv[i])) {
      if (++i >= argc)
        usage();
      BookmarkFile = argv[i];
      if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
        Str tmp = Strnew_charp(CurrentDir);
        if (Strlastchar(tmp) != '/')
          Strcat_char(tmp, '/');
        Strcat_charp(tmp, BookmarkFile);
        BookmarkFile = cleanupName(tmp->ptr);
      }
    } else if (!strcmp("-F", argv[i]))
      RenderFrame = true;
    else if (!strcmp("-W", argv[i])) {
      if (WrapDefault)
        WrapDefault = false;
      else
        WrapDefault = true;
    } else if (!strcmp("-halfload", argv[i])) {
      w3m_halfload = true;
      DefaultType = default_type = "text/html";
    } else if (!strcmp("-ppc", argv[i])) {
      double ppc;
      if (++i >= argc)
        usage();
      ppc = atof(argv[i]);
      if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR) {
        pixel_per_char = ppc;
        set_pixel_per_char = true;
      }
    } else if (!strcmp("-ri", argv[i])) {
      enable_inline_image = INLINE_IMG_OSC5379;
    } else if (!strcmp("-sixel", argv[i])) {
      enable_inline_image = INLINE_IMG_SIXEL;
    } else if (!strcmp("-num", argv[i]))
      showLineNum = true;
    else if (!strcmp("-no-proxy", argv[i]))
      use_proxy = false;
#ifdef INET6
    else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
      set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
#endif
    else if (!strcmp("-post", argv[i])) {
      if (++i >= argc)
        usage();
      post_file = argv[i];
    } else if (!strcmp("-header", argv[i])) {
      Str hs;
      if (++i >= argc)
        usage();
      if ((hs = make_optional_header_string(argv[i])) != NULL) {
        if (header_string == NULL)
          header_string = hs;
        else
          Strcat(header_string, hs);
      }
      while (argv[i][0]) {
        argv[i][0] = '\0';
        argv[i]++;
      }
    } else if (!strcmp("-no-cookie", argv[i])) {
      use_cookie = false;
      accept_cookie = false;
    } else if (!strcmp("-cookie", argv[i])) {
      use_cookie = true;
      accept_cookie = true;
    } else if (!strcmp("-s", argv[i]))
      squeezeBlankLine = true;
    else if (!strcmp("-X", argv[i]))
      Do_not_use_ti_te = true;
    else if (!strcmp("-title", argv[i]))
      displayTitleTerm = getenv("TERM");
    else if (!strncmp("-title=", argv[i], 7))
      displayTitleTerm = argv[i] + 7;
    else if (!strcmp("-insecure", argv[i])) {
#ifdef OPENSSL_TLS_SECURITY_LEVEL
      set_param_option("ssl_cipher=ALL:eNULL:@SECLEVEL=0");
#else
      set_param_option("ssl_cipher=ALL:eNULL");
#endif
#ifdef SSL_CTX_set_min_proto_version
      set_param_option("ssl_min_version=all");
#endif
      set_param_option("ssl_forbid_method=");
      set_param_option("ssl_verify_server=0");
    } else if (!strcmp("-o", argv[i]) || !strcmp("-show-option", argv[i])) {
      if (!strcmp("-show-option", argv[i]) || ++i >= argc ||
          !strcmp(argv[i], "?")) {
        show_params(stdout);
        exit(0);
      }
      if (!set_param_option(argv[i])) {
        /* option set failed */
        /* FIXME: gettextize? */
        fprintf(stderr, "%s: bad option\n", argv[i]);
        show_params_p = 1;
        usage();
      }
    } else if (!strcmp("-", argv[i]) || !strcmp("-dummy", argv[i])) {
      /* do nothing */
    } else if (!strcmp("-debug", argv[i])) {
      w3m_debug = true;
    } 
#if defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE)
    else if (!strcmp("-$$getimage", argv[i])) {
      ++i;
      getimage_args = argv + i;
      i += 4;
      if (i > argc)
        usage();
    }
#endif /* defined(DONT_CALL_GC_AFTER_FORK) && defined(USE_IMAGE) */
    else {
      usage();
    }
  } else if (*argv[i] == '+') {
    line_str = argv[i] + 1;
  } else {
    load_argv[load_argc++] = argv[i];
  }
  i++;
}
