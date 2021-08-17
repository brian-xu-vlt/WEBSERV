#pragma once

# include <iostream>
# include <fstream>
# include <string>
# include <map>
# include <fcntl.h>
# include <fcntl.h>
# include <unistd.h>
# include <sys/stat.h>

# include "Logger.hpp"
# include "Timer.hpp"
# include "Constants.hpp"

namespace files {

	enum Status
	{
		Waiting,
		Ready		// Ready to be sent to be streamed
	};
	enum typeFd
	{
		file = 0,
		pipe = 1
	};

	class File	{

        friend class GetMethod;

		public:


			File( void );
			File( std::string const & path, int flags = O_RDONLY );
			~File( void );

			File &		operator=( File const & rhs );

			bool		isGood(void) const;
            int         getFD( void ) const;
            size_t		getSize( void ) const;
            int         getError( void ) const;

            std::string         getLastModified( void ) const;
            std::string         getType( void ) const;
            std::string         getTypeFromExt( std::string const & ext ) const;
            static bool         isFileFromPath( std::string const & path );
            static std::string	getFileFromPath( std::string const & path );

            static void initContentTypes( char const * pathTypesConf );

		private:

            typedef std::map<std::string, std::string>   typesMap_t;

            static typesMap_t   _types;
            int                 _fd;
			std::string 		_path;
			int          		_error;
			int          		_flags;
			bool          		_typefd;

            void        openFile( void );
			File( File const & src );
		};

} // --- end of namespace fileHandler



/*
    text/html                                        html htm shtml;
    text/css                                         css;
    text/xml                                         xml;
    image/gif                                        gif;
    image/jpeg                                       jpeg jpg;
    application/javascript                           js;
    application/atom+xml                             atom;
    application/rss+xml                              rss;

    text/mathml                                      mml;
    text/plain                                       txt;
    text/vnd.sun.j2me.app-descriptor                 jad;
    text/vnd.wap.wml                                 wml;
    text/x-component                                 htc;

    image/png                                        png;
    image/svg+xml                                    svg svgz;
    image/tiff                                       tif tiff;
    image/vnd.wap.wbmp                               wbmp;
    image/webp                                       webp;
    image/x-icon                                     ico;
    image/x-jng                                      jng;
    image/x-ms-bmp                                   bmp;

    font/woff                                        woff;
    font/woff2                                       woff2;

    application/java-archive                         jar war ear;
    application/json                                 json;
    application/mac-binhex40                         hqx;
    application/msword                               doc;
    application/pdf                                  pdf;
    application/postscript                           ps eps ai;
    application/rtf                                  rtf;
    application/vnd.apple.mpegurl                    m3u8;
    application/vnd.google-earth.kml+xml             kml;
    application/vnd.google-earth.kmz                 kmz;
    application/vnd.ms-excel                         xls;
    application/vnd.ms-fontobject                    eot;
    application/vnd.ms-powerpoint                    ppt;
    application/vnd.oasis.opendocument.graphics      odg;
    application/vnd.oasis.opendocument.presentation  odp;
    application/vnd.oasis.opendocument.spreadsheet   ods;
    application/vnd.oasis.opendocument.text          odt;
    application/vnd.openxmlformats-officedocument.presentationml.presentation   pptx;
    application/vnd.openxmlformats-officedocument.spreadsheetml.sheet           xlsx;
    application/vnd.openxmlformats-officedocument.wordprocessingml.document     docx;
    application/vnd.wap.wmlc                         wmlc;
    application/x-7z-compressed                      7z;
    application/x-cocoa                              cco;
    application/x-java-archive-diff                  jardiff;
    application/x-java-jnlp-file                     jnlp;
    application/x-makeself                           run;
    application/x-perl                               pl pm;
    application/x-pilot                              prc pdb;
    application/x-rar-compressed                     rar;
    application/x-redhat-package-manager             rpm;
    application/x-sea                                sea;
    application/x-shockwave-flash                    swf;
    application/x-stuffit                            sit;
    application/x-tcl                                tcl tk;
    application/x-x509-ca-cert                       der pem crt;
    application/x-xpinstall                          xpi;
    application/xhtml+xml                            xhtml;
    application/xspf+xml                             xspf;
    application/zip                                  zip;

    application/octet-stream                         bin exe dll;
    application/octet-stream                         deb;
    application/octet-stream                         dmg;
    application/octet-stream                         iso img;
    application/octet-stream                         msi msp msm;

    audio/midi                                       mid midi kar;
    audio/mpeg                                       mp3;
    audio/ogg                                        ogg;
    audio/x-m4a                                      m4a;
    audio/x-realaudio                                ra;

    video/3gpp                                       3gpp 3gp;
    video/mp2t                                       ts;
    video/mp4                                        mp4;
    video/mpeg                                       mpeg mpg;
    video/quicktime                                  mov;
    video/webm                                       webm;
    video/x-flv                                      flv;
    video/x-m4v                                      m4v;
    video/x-mng                                      mng;
    video/x-ms-asf                                   asx asf;
    video/x-ms-wmv                                   wmv;
    video/x-msvideo                                  avi;
*/
