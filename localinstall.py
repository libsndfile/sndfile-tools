#!/usr/bin/python

import os, re, string, sys, time

# AC_INIT(libsndfile,1.1.0pre1,<erikd@mega-nerd.com>)
def find_configure_version (filename):
	file = open (filename)
	while 1:
		line = file.readline ()
		if re.search ("AC_INIT\(", line):
			line = re.sub ("AC_INIT\(", "", line)
			x = re.sub ("\).*\n", "", line)
			package, version, rubbish = string.split (x, ",", 3)
			package = re.sub ("[\[\]]", "", package)
			version = re.sub ("[\[\]]", "", version)
			break
	file.close ()
	# version = re.escape (version)
	return package, version
	
def find_header_version (filename):
	file = open (filename)
	while 1:
		line = file.readline ()
		if not line:
			print "Error : could not find version in :", filename
			sys.exit (1)
		if re.search ("#define[ \t]+VERSION", line):
			version = re.sub ("[#a-zA-Z \t]+\"", "", line)
			version = re.sub ("\".*\n", "", version)
			break
	file.close ()
	# version = re.escape (version)
	return version
	
def html_find_configure_version (data):
	if os.path.isfile (data):
		file = open (data, "r")
		data = file.read ()
		file.close ()
	version = re.search ("<META NAME=\"Version\"[ \t]+CONTENT=\"sndfile-tools-[^\"]+\">", data)
	version = re.search ("sndfile-tools-[^\"]+\"", version.group (0))
	version = re.sub ("(sndfile-tools-|\")", "", version.group (0))
	return version

def html_fix_version (filename, new_version):
	file = open (filename, "r")
	data = file.read ()
	file.close () 
	temp_filename = "%s.%d" % (filename, int (time.time ()))
	os.rename (filename, temp_filename)
	html_version = html_find_configure_version (data)
	
	try:
		if html_version != new_version:
			print "Updating html file version number."
	
			data = re.sub (re.escape ("-%s" % html_version), "-%s" % new_version, data)

			file = open (filename, "w")
			file.write (data)
			file.close ()
			os.unlink (temp_filename)
		else:
			os.rename (temp_filename, filename)
			
	except:
		print "Html file update failed."
		os.rename (temp_filename, filename)
		sys.exit (1)

#print html_find_configure_version ("doc/index.html")
#
#sys.exit (0)


#=========================================================================
# Main program.

conf_package, conf_version = find_configure_version ("configure.ac")

header_version = find_header_version ("src/config.h")

if conf_version != header_version:
	print "Configure/header version mismatch!", conf_version, header_version
	sys.exit (0)
	
# os.system ("mgdiff Win32/sndfile.h src/sndfile.h")

os.system ("nedit AUTHORS NEWS README ChangeLog")

os.system ("nedit doc/*.html")

# Generate the filenames from the versions.
	
tar_gz_file = "%s-%s.tar.gz" % (conf_package, conf_version)

if not os.path.isfile (tar_gz_file):
	print "No tarball. Exiting!"
	sys.exit (1)

#if not os.path.isfile (tar_gz_file + ".asc"):
#	print "No GPG signature for tarball. Exiting!"
#	print "gpg -a -detach-sign file"
#	sys.exit (1)

# Edit the html files :-)
html_fix_version ("doc/index.html", conf_version)

if not os.path.isfile ("doc/index.html"):
	print "\n\n************* No doc/index.html ****************\n"
	sys.exit (1)

if len(sys.argv) == 2 and sys.argv [1] == "--copy":
	print "**************** Copying to Web Directory. ****************" ;
	dest = "/home/erikd/HTML/Mega-Nerd/libsndfile/tools/"

	os.system ("cp -f AUTHORS NEWS README ChangeLog " + dest)

	os.system (("cp -f doc/*.html doc/*.png %s " + dest) % (tar_gz_file))
	os.system ("chmod a+r /home/erikd/HTML/Mega-Nerd/libsndfile/tools/*")
	sys.exit (0)
else:
	print "Not copying."

