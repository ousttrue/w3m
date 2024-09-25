void init_rc(void) {
  int i;
  struct stat st;
  FILE *f;

  if (rc_dir != NULL)
    goto open_rc;

  rc_dir = expandPath(RC_DIR);
  i = strlen(rc_dir);
  if (i > 1 && rc_dir[i - 1] == '/')
    rc_dir[i - 1] = '\0';

  if (stat(rc_dir, &st) < 0) {
    if (errno == ENOENT) { /* no directory */
      if (mkdir(rc_dir, 0700) < 0) {
        /* fprintf(stderr, "Can't create config directory (%s)!\n", rc_dir); */
        goto rc_dir_err;
      } else {
        stat(rc_dir, &st);
      }
    } else {
      /* fprintf(stderr, "Can't open config directory (%s)!\n", rc_dir); */
      goto rc_dir_err;
    }
  }
  if (!S_ISDIR(st.st_mode)) {
    /* not a directory */
    /* fprintf(stderr, "%s is not a directory!\n", rc_dir); */
    goto rc_dir_err;
  }
  if (!(st.st_mode & S_IWUSR)) {
    /* fprintf(stderr, "%s is not writable!\n", rc_dir); */
    goto rc_dir_err;
  }
  no_rc_dir = FALSE;
  tmp_dir = rc_dir;

  if (config_file == NULL)
    config_file = rcFile(CONFIG_FILE);

  create_option_search_table();

open_rc:
  /* open config file */
  if ((f = fopen(etcFile(W3MCONFIG), "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
  if ((f = fopen(confFile(CONFIG_FILE), "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
  if (config_file && (f = fopen(config_file, "rt")) != NULL) {
    interpret_rc(f);
    fclose(f);
  }
  return;

rc_dir_err:
  no_rc_dir = TRUE;
  if (((tmp_dir = getenv("TMPDIR")) == NULL || *tmp_dir == '\0') &&
      ((tmp_dir = getenv("TMP")) == NULL || *tmp_dir == '\0') &&
      ((tmp_dir = getenv("TEMP")) == NULL || *tmp_dir == '\0'))
    tmp_dir = "/tmp";
#ifdef HAVE_MKDTEMP
  tmp_dir = mkdtemp(Strnew_m_charp(tmp_dir, "/w3m-XXXXXX", NULL)->ptr);
  if (tmp_dir == NULL)
    tmp_dir = rc_dir;
#endif
  create_option_search_table();
  goto open_rc;
}


