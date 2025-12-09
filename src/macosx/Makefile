TOPLEVEL=../..

include ../Make.inc

all: $(BUILD)/fuse-xfs-$(VERSION).dmg

PACKAGING=Description.plist Info.plist.in Install_resources add_background.osa fuse-xfs.fs make-pkg.sh

$(DMG): $(PROGRAMS) $(PACKAGING)
	sh make-pkg.sh $(VERSION) $(TOPLEVEL) $(BUILD) .

clean:

