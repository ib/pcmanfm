desktopdir = $(datadir)/applications
desktop_in_files = pcmanfm.desktop.in \
	       pcmanfm-find.desktop.in \
	       pcmanfm-folder-handler.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
DISTCLEANFILES = pcmanfm.desktop \
		 pcmanfm-find.desktop \
		 pcmanfm-folder-handler.desktop

icondir = $(datadir)/pixmaps
icon_DATA = pcmanfm.png

filedir = $(datadir)/pcmanfm
uidir=$(filedir)/ui

file_DATA = \
	mount.rules

# GtkBuilder UI definition files
ui_in_files= \
	ui/prefdlg.glade \
	ui/appchooserdlg.glade \
	ui/file-assoc-dlg.glade \
	ui/file-properties.glade \
	ui/about-dlg.glade \
	ui/find-files.glade \
	ui/godlg.glade
ui_DATA = $(ui_in_files:.glade=.ui)

EXTRA_DIST = \
	$(desktop_DATA) \
	$(icon_DATA) \
	$(file_DATA) \
	$(ui_DATA)

CLEANFILES = \
	$(ui_DATA)

# Generate GtkBuilder UI files from Glade files
%.ui: %.glade
#	$(GTK_BUILDER_CONVERT) $< $@
	$(top_srcdir)/gtk-builder-convert $< $@
#	$(top_builddir)/src/xml-purge $@
