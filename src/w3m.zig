pub const c = @cImport({
    @cInclude("w3m_runtime.h");
    @cInclude("w3m_config.h");
    @cInclude("local_cgi.h");
    @cInclude("image.h");
});
