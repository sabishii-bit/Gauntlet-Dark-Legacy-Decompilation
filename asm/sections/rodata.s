.include "macros.s"

.section .rodata, "wa"  # 0x80110720 - 0x80118100 ; 0x000079E0


.global lbl_80110720
lbl_80110720:

	# ROM: 0x10D720
	.asciz "GRID ERROR"
	.balign 4
	.4byte 0

.global lbl_80110730
lbl_80110730:

	# ROM: 0x10D730
	.asciz "Init Anim Failed: %d NEXT: %d MAX: %d"
	.balign 4

.global lbl_80110758
lbl_80110758:

	# ROM: 0x10D758
	.asciz "Init Anim: seq %d > Max %d Using 0"
	.balign 4
	.4byte 0

.global lbl_80110780
lbl_80110780:

	# ROM: 0x10D780
	.asciz "AtreeFindNodeIdx can not find node %s"
	.balign 4

.global lbl_801107A8
lbl_801107A8:

	# ROM: 0x10D7A8
	.asciz "AtreeMatch with NULL atree"
	.balign 4

.global lbl_801107C4
lbl_801107C4:

	# ROM: 0x10D7C4
	.asciz "No AtreeMatch: %s"
	.balign 4

.global lbl_801107D8
lbl_801107D8:

	# ROM: 0x10D7D8
	.asciz "Too many Atrees\n"
	.balign 4
	.asciz "> MAX NODES IN ATREE"
	.balign 4
	.asciz "ERROR ADDING NEW ANODES"
	.asciz "NODE HAS PARENT > NODE"
	.balign 4
	.asciz "NODE HAS PARENT BEFORE ROOT"
	.asciz "ATREE: %s HAS MULTIPLE ROOTS: %d AND %d"
	.asciz "ATREE HAS NO ROOT"
	.balign 4

.global lbl_8011088C
lbl_8011088C:

	# ROM: 0x10D88C
	.asciz "Attempt to add > %d Atree nodes"

.global lbl_801108AC
lbl_801108AC:

	# ROM: 0x10D8AC
	.asciz "AtreeNodeInit: MBNewObject return 0 (too many nodes)"
	.balign 4

.global lbl_801108E4
lbl_801108E4:

	# ROM: 0x10D8E4
	.asciz "Too Many AnimDataNodes"
	.balign 4
	.4byte 0

.global lbl_80110900
lbl_80110900:

	# ROM: 0x10D900
	.asciz "MIDWAY GAMES WEST"
	.balign 4

.global lbl_80110914
lbl_80110914:

	# ROM: 0x10D914
	.asciz "Lead Programmer"

.global lbl_80110924
lbl_80110924:

	# ROM: 0x10D924
	.asciz "  Steven Bennetts"
	.balign 4

.global lbl_80110938
lbl_80110938:

	# ROM: 0x10D938
	.asciz "Art Director"
	.balign 4

.global lbl_80110948
lbl_80110948:

	# ROM: 0x10D948
	.asciz "  Steve \"Scat\" Caterson"

.global lbl_80110960
lbl_80110960:

	# ROM: 0x10D960
	.asciz "Producer"
	.balign 4

.global lbl_8011096C
lbl_8011096C:

	# ROM: 0x10D96C
	.asciz "  Mike Hally"
	.balign 4

.global lbl_8011097C
lbl_8011097C:

	# ROM: 0x10D97C
	.asciz "Lead 3D Artist"
	.balign 4

.global lbl_8011098C
lbl_8011098C:

	# ROM: 0x10D98C
	.asciz "  Don Livingston"
	.balign 4

.global lbl_801109A0
lbl_801109A0:

	# ROM: 0x10D9A0
	.asciz "3D Artists"
	.balign 4

.global lbl_801109AC
lbl_801109AC:

	# ROM: 0x10D9AC
	.asciz "  Stig Asmussen"

.global lbl_801109BC
lbl_801109BC:

	# ROM: 0x10D9BC
	.asciz "  Rhizaldi Bugawan"
	.balign 4

.global lbl_801109D0
lbl_801109D0:

	# ROM: 0x10D9D0
	.asciz "  Chris Sutton"
	.balign 4

.global lbl_801109E0
lbl_801109E0:

	# ROM: 0x10D9E0
	.asciz "  Paul Haskins"
	.balign 4

.global lbl_801109F0
lbl_801109F0:

	# ROM: 0x10D9F0
	.asciz "Character Animation"

.global lbl_80110A04
lbl_80110A04:

	# ROM: 0x10DA04
	.asciz "  Takeshi Hasegawa"
	.balign 4

.global lbl_80110A18
lbl_80110A18:

	# ROM: 0x10DA18
	.asciz "  Gustavo Rasche"
	.balign 4

.global lbl_80110A2C
lbl_80110A2C:

	# ROM: 0x10DA2C
	.asciz "Programmers"

.global lbl_80110A38
lbl_80110A38:

	# ROM: 0x10DA38
	.asciz "  Nathan \"Acorn\" Pooley"

.global lbl_80110A50
lbl_80110A50:

	# ROM: 0x10DA50
	.asciz "  Jonathan Hudgins"
	.balign 4

.global lbl_80110A64
lbl_80110A64:

	# ROM: 0x10DA64
	.asciz "  Lennard Feddersen"

.global lbl_80110A78
lbl_80110A78:

	# ROM: 0x10DA78
	.asciz "  Tyler Voltier"

.global lbl_80110A88
lbl_80110A88:

	# ROM: 0x10DA88
	.asciz "Game Design"

.global lbl_80110A94
lbl_80110A94:

	# ROM: 0x10DA94
	.asciz "  KJ Holm"
	.balign 4

.global lbl_80110AA0
lbl_80110AA0:

	# ROM: 0x10DAA0
	.asciz "Music and Sound Design"
	.balign 4

.global lbl_80110AB8
lbl_80110AB8:

	# ROM: 0x10DAB8
	.asciz "  Joe Lyford"
	.balign 4

.global lbl_80110AC8
lbl_80110AC8:

	# ROM: 0x10DAC8
	.asciz "Audio Programmer"
	.balign 4

.global lbl_80110ADC
lbl_80110ADC:

	# ROM: 0x10DADC
	.asciz "  Sean Gugler"
	.balign 4

.global lbl_80110AEC
lbl_80110AEC:

	# ROM: 0x10DAEC
	.asciz "Executive Producer"
	.balign 4

.global lbl_80110B00
lbl_80110B00:

	# ROM: 0x10DB00
	.asciz "  Dan Van Elderen"
	.balign 4

.global lbl_80110B14
lbl_80110B14:

	# ROM: 0x10DB14
	.asciz "GAME TESTING"
	.balign 4

.global lbl_80110B24
lbl_80110B24:

	# ROM: 0x10DB24
	.asciz "QA/Test Manager"

.global lbl_80110B34
lbl_80110B34:

	# ROM: 0x10DB34
	.asciz "  Larry Cadelina"
	.balign 4

.global lbl_80110B48
lbl_80110B48:

	# ROM: 0x10DB48
	.asciz "QA/Test Supervisor"
	.balign 4

.global lbl_80110B5C
lbl_80110B5C:

	# ROM: 0x10DB5C
	.asciz "  Pele Gaoteote"

.global lbl_80110B6C
lbl_80110B6C:

	# ROM: 0x10DB6C
	.asciz "Lead Tester"

.global lbl_80110B78
lbl_80110B78:

	# ROM: 0x10DB78
	.asciz "  Charles \"Darth\" Ybarra"
	.balign 4

.global lbl_80110B94
lbl_80110B94:

	# ROM: 0x10DB94
	.asciz "Product Analysts"
	.balign 4

.global lbl_80110BA8
lbl_80110BA8:

	# ROM: 0x10DBA8
	.asciz "  Trenton Lewis"

.global lbl_80110BB8
lbl_80110BB8:

	# ROM: 0x10DBB8
	.asciz "  Spencer Ray"
	.balign 4

.global lbl_80110BC8
lbl_80110BC8:

	# ROM: 0x10DBC8
	.asciz "  Joffrey Suarez"
	.balign 4

.global lbl_80110BDC
lbl_80110BDC:

	# ROM: 0x10DBDC
	.asciz "  Pablo Buitrago"
	.balign 4

.global lbl_80110BF0
lbl_80110BF0:

	# ROM: 0x10DBF0
	.asciz "  Justin Hendry"

.global lbl_80110C00
lbl_80110C00:

	# ROM: 0x10DC00
	.asciz "  Mat Kutaka"
	.balign 4

.global lbl_80110C10
lbl_80110C10:

	# ROM: 0x10DC10
	.asciz "  Chip Lyas"

.global lbl_80110C1C
lbl_80110C1C:

	# ROM: 0x10DC1C
	.asciz "  Fredric Mora"
	.balign 4

.global lbl_80110C2C
lbl_80110C2C:

	# ROM: 0x10DC2C
	.asciz "  Shawn Parmer"
	.balign 4

.global lbl_80110C3C
lbl_80110C3C:

	# ROM: 0x10DC3C
	.asciz "  Jake Rainbow"
	.balign 4

.global lbl_80110C4C
lbl_80110C4C:

	# ROM: 0x10DC4C
	.asciz "  CJ Sarraga"
	.balign 4

.global lbl_80110C5C
lbl_80110C5C:

	# ROM: 0x10DC5C
	.asciz "  Alex Suarez"
	.balign 4

.global lbl_80110C6C
lbl_80110C6C:

	# ROM: 0x10DC6C
	.asciz "  Marvin Hale"
	.balign 4

.global lbl_80110C7C
lbl_80110C7C:

	# ROM: 0x10DC7C
	.asciz "SPECIAL THANKS"
	.balign 4

.global lbl_80110C8C
lbl_80110C8C:

	# ROM: 0x10DC8C
	.asciz "Additional High Res Assets Provided by"
	.balign 4

.global lbl_80110CB4
lbl_80110CB4:

	# ROM: 0x10DCB4
	.asciz "  Giant Studios"

.global lbl_80110CC4
lbl_80110CC4:

	# ROM: 0x10DCC4
	.asciz "  Viewpoint Digital"

.global lbl_80110CD8
lbl_80110CD8:

	# ROM: 0x10DCD8
	.asciz "Additional Music and Sound Provided by"
	.balign 4

.global lbl_80110D00
lbl_80110D00:

	# ROM: 0x10DD00
	.asciz " Michael Henry"
	.balign 4

.global lbl_80110D10
lbl_80110D10:

	# ROM: 0x10DD10
	.asciz " John Paul"
	.balign 4

.global lbl_80110D1C
lbl_80110D1C:

	# ROM: 0x10DD1C
	.asciz " Barry Leitch"
	.balign 4

.global lbl_80110D2C
lbl_80110D2C:

	# ROM: 0x10DD2C
	.asciz " Don Diekneite"
	.balign 4

.global lbl_80110D3C
lbl_80110D3C:

	# ROM: 0x10DD3C
	.asciz "Voice Talent"
	.balign 4

.global lbl_80110D4C
lbl_80110D4C:

	# ROM: 0x10DD4C
	.asciz " Douglas Lawrence"
	.balign 4

.global lbl_80110D60
lbl_80110D60:

	# ROM: 0x10DD60
	.asciz "Other Voice Talent by"
	.balign 4

.global lbl_80110D78
lbl_80110D78:

	# ROM: 0x10DD78
	.asciz "Lani Minella"
	.balign 4

.global lbl_80110D88
lbl_80110D88:

	# ROM: 0x10DD88
	.asciz "Wally Fields"
	.balign 4

.global lbl_80110D98
lbl_80110D98:

	# ROM: 0x10DD98
	.asciz "Tox Gunn"
	.balign 4

.global lbl_80110DA4
lbl_80110DA4:

	# ROM: 0x10DDA4
	.asciz "Patrice Crawford"
	.balign 4

.global lbl_80110DB8
lbl_80110DB8:

	# ROM: 0x10DDB8
	.asciz "Female Choir Performed by"
	.balign 4

.global lbl_80110DD4
lbl_80110DD4:

	# ROM: 0x10DDD4
	.asciz "Mari Sanders"
	.balign 4

.global lbl_80110DE4
lbl_80110DE4:

	# ROM: 0x10DDE4
	.asciz "Rachel Nador"
	.balign 4

.global lbl_80110DF4
lbl_80110DF4:

	# ROM: 0x10DDF4
	.asciz "Breath of Giants courtesy of Jim Geist"
	.balign 4

.global lbl_80110E1C
lbl_80110E1C:

	# ROM: 0x10DE1C
	.asciz "Journey and Wilderness courtesy of Scott Snyder"

.global lbl_80110E4C
lbl_80110E4C:

	# ROM: 0x10DE4C
	.asciz "Beauty Queen, Ham, Lead Head, Take Care of the Lonely"
	.balign 4

.global lbl_80110E84
lbl_80110E84:

	# ROM: 0x10DE84
	.asciz "courtesy of TrueHuman"
	.balign 4

.global lbl_80110E9C
lbl_80110E9C:

	# ROM: 0x10DE9C
	.asciz "MIDWAY HOME ENTERTAINMENT"
	.balign 4

.global lbl_80110EB8
lbl_80110EB8:

	# ROM: 0x10DEB8
	.asciz "  Kevin Potter"
	.balign 4

.global lbl_80110EC8
lbl_80110EC8:

	# ROM: 0x10DEC8
	.asciz "Associate Producer"
	.balign 4

.global lbl_80110EDC
lbl_80110EDC:

	# ROM: 0x10DEDC
	.asciz "  Sean Wilson"
	.balign 4

.global lbl_80110EEC
lbl_80110EEC:

	# ROM: 0x10DEEC
	.asciz "Assistant Producer"
	.balign 4

.global lbl_80110F00
lbl_80110F00:

	# ROM: 0x10DF00
	.asciz "  Matthew Vella"

.global lbl_80110F10
lbl_80110F10:

	# ROM: 0x10DF10
	.asciz "  Nico Bihary"
	.balign 4

.global lbl_80110F20
lbl_80110F20:

	# ROM: 0x10DF20
	.asciz "Test Manager"
	.balign 4

.global lbl_80110F30
lbl_80110F30:

	# ROM: 0x10DF30
	.asciz "  Rob Sablan"
	.balign 4

.global lbl_80110F40
lbl_80110F40:

	# ROM: 0x10DF40
	.asciz "Test Supervisor"

.global lbl_80110F50
lbl_80110F50:

	# ROM: 0x10DF50
	.asciz "  Dan Wagner"
	.balign 4

.global lbl_80110F60
lbl_80110F60:

	# ROM: 0x10DF60
	.asciz "Technical Standards Analysts"
	.balign 4

.global lbl_80110F80
lbl_80110F80:

	# ROM: 0x10DF80
	.asciz "  Adam Bailey"
	.balign 4

.global lbl_80110F90
lbl_80110F90:

	# ROM: 0x10DF90
	.asciz "  Rick Blair"
	.balign 4

.global lbl_80110FA0
lbl_80110FA0:

	# ROM: 0x10DFA0
	.asciz "  Matt Jenkins"
	.balign 4

.global lbl_80110FB0
lbl_80110FB0:

	# ROM: 0x10DFB0
	.asciz "Assistant Lead Product Analyst"
	.balign 4

.global lbl_80110FD0
lbl_80110FD0:

	# ROM: 0x10DFD0
	.asciz "  Chanel Penley"

.global lbl_80110FE0
lbl_80110FE0:

	# ROM: 0x10DFE0
	.asciz "  John Bozeman"
	.balign 4

.global lbl_80110FF0
lbl_80110FF0:

	# ROM: 0x10DFF0
	.asciz "  Myong Hong"
	.balign 4

.global lbl_80111000
lbl_80111000:

	# ROM: 0x10E000
	.asciz "  Dave Silva"
	.balign 4

.global lbl_80111010
lbl_80111010:

	# ROM: 0x10E010
	.asciz "  Justin Vancho"

.global lbl_80111020
lbl_80111020:

	# ROM: 0x10E020
	.asciz "Print Design & Production"
	.balign 4

.global lbl_8011103C
lbl_8011103C:

	# ROM: 0x10E03C
	.asciz "  Creative Services - San Diego, CA"

.global lbl_80111060
lbl_80111060:

	# ROM: 0x10E060
	.asciz "V.P. of Marketing"
	.balign 4

.global lbl_80111074
lbl_80111074:

	# ROM: 0x10E074
	.asciz "  Helene Sheeler"
	.balign 4

.global lbl_80111088
lbl_80111088:

	# ROM: 0x10E088
	.asciz "Director of Entertainment Marketing"

.global lbl_801110AC
lbl_801110AC:

	# ROM: 0x10E0AC
	.asciz "  Lawrence Smith"
	.balign 4

.global lbl_801110C0
lbl_801110C0:

	# ROM: 0x10E0C0
	.asciz "Product Manager"

.global lbl_801110D0
lbl_801110D0:

	# ROM: 0x10E0D0
	.asciz "  Dennis Roy"
	.balign 4

.global lbl_801110E0
lbl_801110E0:

	# ROM: 0x10E0E0
	.asciz "Special Thanks"
	.balign 4

.global lbl_801110F0
lbl_801110F0:

	# ROM: 0x10E0F0
	.asciz "  Deborah Fulton"
	.balign 4

.global lbl_80111104
lbl_80111104:

	# ROM: 0x10E104
	.asciz "  Rob Gustufson"

.global lbl_80111114
lbl_80111114:

	# ROM: 0x10E114
	.asciz "  May Cam"
	.balign 4

.global lbl_80111120
lbl_80111120:

	# ROM: 0x10E120
	.asciz "  Patrick Dillon"
	.balign 4

.global lbl_80111134
lbl_80111134:

	# ROM: 0x10E134
	.asciz "BIG APE PRODUCTIONS INC."
	.balign 4

.global lbl_80111150
lbl_80111150:

	# ROM: 0x10E150
	.asciz "Bob Arient"
	.balign 4

.global lbl_8011115C
lbl_8011115C:

	# ROM: 0x10E15C
	.asciz "Mike Schlachter"

.global lbl_8011116C
lbl_8011116C:

	# ROM: 0x10E16C
	.asciz "Additional Support"
	.balign 4

.global lbl_80111180
lbl_80111180:

	# ROM: 0x10E180
	.asciz "Michael Ebert"
	.balign 4

.global lbl_80111190
lbl_80111190:

	# ROM: 0x10E190
	.asciz "Patrick Shaw"
	.balign 4
	.asciz "Press Start"
	.asciz "Loading..."
	.balign 4

.global lbl_801111B8
lbl_801111B8:

	# ROM: 0x10E1B8
	.asciz "AAANULLOBJ"
	.balign 4
	.asciz "Load titlescreen.\n"
	.balign 4
	.asciz "Audio Stop.\n"
	.balign 4
	.asciz "Audio Select.\n"
	.balign 4
	.asciz "GLOWCROP_00"
	.asciz "Load select.\n"
	.balign 4
	.asciz "Alloc mem.\n"
	.asciz "Init titlescreen done.\n"

.global lbl_80111238
lbl_80111238:

	# ROM: 0x10E238
	.asciz "Version %s "
	.asciz "CRED_ALL%02d"
	.balign 4
	.asciz "castle2b"
	.balign 4
	.asciz "FONT32GAR0"
	.balign 4
	.asciz "Press\nStart"

.global lbl_80111278
lbl_80111278:

	# ROM: 0x10E278
	.asciz "Scroll_A"
	.balign 4
	.asciz "tex_mip_k = %d\n"

.global lbl_80111294
lbl_80111294:

	# ROM: 0x10E294
	.asciz "Screen2D"
	.balign 4

.global lbl_801112A0
lbl_801112A0:

	# ROM: 0x10E2A0
	.asciz "Titlescreen"

.global lbl_801112AC
lbl_801112AC:

	# ROM: 0x10E2AC
	.asciz "Init moving objects.\n"
	.balign 4
	.4byte 0

.global lbl_801112C8
lbl_801112C8:

	# ROM: 0x10E2C8
	.asciz "Audio Play Timeout"
	.balign 4

.global lbl_801112DC
lbl_801112DC:

	# ROM: 0x10E2DC
	.asciz "AUDIO: BANK %s NOT LOADED. SOUND:%s\n"
	.balign 4

.global lbl_80111304
lbl_80111304:

	# ROM: 0x10E304
	.asciz "AUDIO: UNABLE TO FIND MODE %s"
	.balign 4

.global lbl_80111324
lbl_80111324:

	# ROM: 0x10E324
	.asciz "RESETTING AUDIO AND TRYING AGAIN"
	.balign 4

.global lbl_80111348
lbl_80111348:

	# ROM: 0x10E348
	.asciz "audatps2.rom"
	.balign 4
	.asciz "Audio Bank bad file: %s"
	.asciz "aud_load_bank failed: %d"
	.balign 4
	.asciz "aud_load_bank failed"
	.balign 4
	.asciz "Audio Stream bad file: %s"
	.balign 4
	.asciz "Audio Stream no buffer memory: %s"
	.balign 4
	.asciz "Audio Stream Err: %s"
	.balign 4

.global lbl_801113FC
lbl_801113FC:

	# ROM: 0x10E3FC
	.asciz "aud_stream_stop failed: %d"
	.balign 4

.global lbl_80111418
lbl_80111418:

	# ROM: 0x10E418
	.asciz "AudioStreamStop: Timeout"
	.balign 4

.global lbl_80111434
lbl_80111434:

	# ROM: 0x10E434
	.asciz "DCS Audio Bank load failed:%s (%d): %d"
	.balign 4

.global lbl_8011145C
lbl_8011145C:

	# ROM: 0x10E45C
	.asciz "AudioUnloadPart skipped bank, still in use\n"

.global lbl_80111488
lbl_80111488:

	# ROM: 0x10E488
	.asciz "Audio Reset Error\n"
	.balign 4

.global lbl_8011149C
lbl_8011149C:

	# ROM: 0x10E49C
	.asciz "Audio Busy = -2"

.global lbl_801114AC
lbl_801114AC:

	# ROM: 0x10E4AC
	.asciz "AUDIO sfx volume can't be less than 0  (%d)\n"
	.balign 4

.global lbl_801114DC
lbl_801114DC:

	# ROM: 0x10E4DC
	.asciz "AUDIO sfx volume can't be greater than %d  (%d)\n"
	.balign 4

.global lbl_80111510
lbl_80111510:

	# ROM: 0x10E510
	.asciz "AUDIO music volume can't be less than 0  (%d)\n"
	.balign 4

.global lbl_80111540
lbl_80111540:

	# ROM: 0x10E540
	.asciz "AUDIO music volume can't be greater than %d  (%d)\n"
	.balign 4

.global lbl_80111574
lbl_80111574:

	# ROM: 0x10E574
	.asciz "UNABLE TO FIND SOUND: %s\n"
	.balign 4
	.asciz "C NAME               PRI VOL PAN FRAME  END   "
	.balign 4
	.asciz "- ------------------ --- --- --- ------ ------"
	.balign 4
	.asciz "%d %-18s  %02X %3d %3d %6d %6d"
	.balign 4
	.asciz "%d                                             "

.global lbl_80111640
lbl_80111640:

	# ROM: 0x10E640
	.asciz "> Max %d Special Texmods"
	.balign 4

.global lbl_8011165C
lbl_8011165C:

	# ROM: 0x10E65C
	.asciz "TEXMOD with 0 texidx, tex:%s srx:%s"

.global lbl_80111680
lbl_80111680:

	# ROM: 0x10E680
	.asciz "> Max %d scrolling textures"

.global lbl_8011169C
lbl_8011169C:

	# ROM: 0x10E69C
	.asciz "Bad header passed in to InitOAnimList."
	.balign 4

.global lbl_801116C4
lbl_801116C4:

	# ROM: 0x10E6C4
	.asciz "InitOAnimList: Unable to find %s (%d)"
	.balign 4
	.4byte 0

.global lbl_801116F0
lbl_801116F0:

	# ROM: 0x10E6F0
	.asciz "dash_%s_%d"
	.balign 4

.global lbl_801116FC
lbl_801116FC:

	# ROM: 0x10E6FC
	.asciz "Loading..."
	.balign 4
	.asciz "maps/level%s"
	.balign 4
	.asciz "map_%s_%02d"
	.asciz "ldmap_%s_%02d"
	.balign 4
	.asciz "TRANSITION_SCREEN"
	.balign 4

.global lbl_80111748
lbl_80111748:

	# ROM: 0x10E748
	.asciz "COIN_BRONZE"
	.asciz "COIN_SILVER"
	.asciz "COIN_GOLD"
	.balign 4
	.asciz "Too many health meters: %d"
	.balign 4
	.asciz "%s%sMETER_BG%d"
	.balign 4
	.asciz "%s%sMETER_FG%d"
	.balign 4

.global lbl_801117A8
lbl_801117A8:

	# ROM: 0x10E7A8
	.asciz "BOSSKEY2"
	.balign 4
	.4byte 0

.global lbl_801117B8
lbl_801117B8:

	# ROM: 0x10E7B8
	.asciz "BossCamStartCalc called with no boss"
	.balign 4

.global lbl_801117E0
lbl_801117E0:

	# ROM: 0x10E7E0
	.asciz "BCAM Y:%.0f P:%.0f D:%.2f(%.2f)  ATN:%.1f %.1f %.1f  "
	.balign 4

.global lbl_80111818
lbl_80111818:

	# ROM: 0x10E818
	.asciz "BCAM DY:%.2f DD:%.2f DP:%.2f"
	.balign 4

.global lbl_80111838
lbl_80111838:

	# ROM: 0x10E838
	.asciz "BCAM DY:%.2f DD:%.2f DP:%.2f "
	.balign 4

.global lbl_80111858
lbl_80111858:

	# ROM: 0x10E858
	.asciz " ATN:%.2Lf %.2Lf %.2Lf"
	.balign 4

.global lbl_80111870
lbl_80111870:

	# ROM: 0x10E870
	.asciz " REF:%.2f %.2f %.2f"
	.asciz "YAW1: %.2f VEC1: %.2f %.2f %.2f "
	.balign 4
	.asciz "YAW2: %.2f VEC2: %.2f %.2f %.2f "
	.balign 4
	.4byte 0

.global lbl_801118D0
lbl_801118D0:

	# ROM: 0x10E8D0
	.asciz "8Hifonts"
	.balign 4

.global lbl_801118DC
lbl_801118DC:

	# ROM: 0x10E8DC
	.asciz "initials"
	.balign 4

.global lbl_801118E8
lbl_801118E8:

	# ROM: 0x10E8E8
	.asciz "scoratt8"
	.balign 4

.global lbl_801118F4
lbl_801118F4:

	# ROM: 0x10E8F4
	.asciz "namefont"
	.balign 4

.global lbl_80111900
lbl_80111900:

	# ROM: 0x10E900
	.asciz "kanji10a"
	.balign 4

.global lbl_8011190C
lbl_8011190C:

	# ROM: 0x10E90C
	.asciz "kanji10b"
	.balign 4

.global lbl_80111918
lbl_80111918:

	# ROM: 0x10E918
	.asciz "kanji20a"
	.balign 4

.global lbl_80111924
lbl_80111924:

	# ROM: 0x10E924
	.asciz "DrawStringTextMLines: Msg=%d idx=%d > max"
	.balign 4

.global lbl_80111950
lbl_80111950:

	# ROM: 0x10E950
	.asciz "DrawStringTextMulti: Msg=%d idx=%d > max"
	.balign 4

.global lbl_8011197C
lbl_8011197C:

	# ROM: 0x10E97C
	.asciz "DrawScrollText: Msg=%d idx=%d > max"

.global lbl_801119A0
lbl_801119A0:

	# ROM: 0x10E9A0
	.asciz "DrawStringText: Msg=%d idx=%d > max"

.global lbl_801119C4
lbl_801119C4:

	# ROM: 0x10E9C4
	.asciz "SCROLLS%s"
	.balign 4
	.asciz "%s_j.rom"
	.balign 4
	.asciz "%s_e.rom"
	.balign 4
	.asciz "japanese.rom"
	.balign 4
	.asciz "english.rom"
	.4byte 0

.global lbl_80111A08
lbl_80111A08:

	# ROM: 0x10EA08
	.asciz "CONTAINER"
	.balign 4

.global lbl_80111A14
lbl_80111A14:

	# ROM: 0x10EA14
	.asciz "GENERATOR"
	.balign 4

.global lbl_80111A20
lbl_80111A20:

	# ROM: 0x10EA20
	.asciz "ENEMYINFO"
	.balign 4

.global lbl_80111A2C
lbl_80111A2C:

	# ROM: 0x10EA2C
	.asciz "DAMAGETILE"
	.balign 4

.global lbl_80111A38
lbl_80111A38:

	# ROM: 0x10EA38
	.asciz "OBSTACLE"
	.balign 4

.global lbl_80111A44
lbl_80111A44:

	# ROM: 0x10EA44
	.asciz "TRANSPORTER"

.global lbl_80111A50
lbl_80111A50:

	# ROM: 0x10EA50
	.asciz "RUNESTONE"
	.balign 4

.global lbl_80111A5C
lbl_80111A5C:

	# ROM: 0x10EA5C
	.asciz "BRIDGEPAD"
	.balign 4

.global lbl_80111A68
lbl_80111A68:

	# ROM: 0x10EA68
	.asciz "BRIDGESW"
	.balign 4

.global lbl_80111A74
lbl_80111A74:

	# ROM: 0x10EA74
	.asciz "ACTIVESW"
	.balign 4

.global lbl_80111A80
lbl_80111A80:

	# ROM: 0x10EA80
	.asciz "SAFEROCK"
	.balign 4

.global lbl_80111A8C
lbl_80111A8C:

	# ROM: 0x10EA8C
	.asciz "EXP BARREL"
	.balign 4

.global lbl_80111A98
lbl_80111A98:

	# ROM: 0x10EA98
	.asciz "POI BARREL"
	.balign 4

.global lbl_80111AA4
lbl_80111AA4:

	# ROM: 0x10EAA4
	.asciz "CHEST GOLD"
	.balign 4

.global lbl_80111AB0
lbl_80111AB0:

	# ROM: 0x10EAB0
	.asciz "CHEST SLVR"
	.balign 4

.global lbl_80111ABC
lbl_80111ABC:

	# ROM: 0x10EABC
	.asciz "FALLING2SECRET"
	.balign 4
	.asciz "PRE CAM=(%3d,%3d,%3d)"
	.balign 4
	.asciz "PRE ATN=(%3d,%3d,%3d)"
	.balign 4
	.asciz "PLR %d WILL BE OUT"
	.balign 4
	.asciz "                 "
	.balign 4
	.asciz "X=%3d, Y=%3d, C=%d  "
	.balign 4

.global lbl_80111B3C
lbl_80111B3C:

	# ROM: 0x10EB3C
	.asciz "DEST P=%d, Y=%4.1f"
	.balign 4

.global lbl_80111B50
lbl_80111B50:

	# ROM: 0x10EB50
	.asciz "SCROLL_A"
	.balign 4
	.asciz "CAM: %.2f %.2f %.2f    "
	.asciz "ATN: %.2f %.2f %.2f    "
	.asciz "YAW=%.1f(%.2f)    "
	.balign 4
	.asciz "PITCH=%d    "
	.balign 4
	.asciz "DISTANCE:  %.2f    "
	.asciz "FREE        "
	.balign 4
	.asciz "                               "
	.asciz "LOCK        "
	.balign 4
	.asciz "OBJECT      "
	.balign 4
	.asciz "TARGET      "
	.balign 4
	.asciz "POINT       "
	.balign 4
	.asciz "PLAYER %02X   "
	.balign 4
	.asciz "ENEMY %02X    "
	.balign 4
	.asciz "%s (AI=%d)                     "
	.asciz "MILESTONE %02X"
	.balign 4
	.asciz "LOOKOUT %02X  "
	.balign 4
	.asciz "CAMERA %02X   "
	.balign 4
	.asciz "ITEM %02X (%dP)"
	.asciz "%s (%s)                        "
	.asciz "%s (%s-%d) Lv%d Max=%d   "
	.balign 4
	.asciz "%s (%s)                      "
	.balign 4
	.asciz "%s (%d)                      "
	.balign 4
	.asciz "%s                          "
	.balign 4
	.asciz "%s (HP=%d)                    "
	.balign 4
	.asciz "%s (%s)            "
	.asciz "%s (GRP=%d)                  "
	.balign 4
	.asciz "%s (%s: RAD=%d)             "
	.balign 4
	.asciz "UNKNOWN     "
	.balign 4
	.asciz "MODE=%d "
	.balign 4

.global lbl_80111DE0
lbl_80111DE0:

	# ROM: 0x10EDE0
	.4byte 0x3F800000
	.4byte 0x3F774BC7
	.4byte 0x3F774BC7
	.4byte 0x3F5DB22D
	.4byte 0x3F5DB22D

.global lbl_80111DF4
lbl_80111DF4:

	# ROM: 0x10EDF4
	.4byte 0
	.4byte 0x3E849BA6
	.4byte 0xBE849BA6
	.4byte 0x3F000000
	.4byte 0xBF000000

.global lbl_80111E08
lbl_80111E08:

	# ROM: 0x10EE08
	.asciz "ENEMY %d HAS NO MISSILE TYPE %d"

.global lbl_80111E28
lbl_80111E28:

	# ROM: 0x10EE28
	.asciz "ERROR: ZERO LENGTH MISSILE VEL"
	.balign 4
	.asciz "%s_THROW0"
	.balign 4
	.asciz "%s_THROW%c"
	.balign 4
	.asciz "%s_THROW1"
	.balign 4
	.asciz "Player Missile not found: %s"
	.balign 4
	.asciz "WEAP_HOLD_%s"
	.balign 4
	.asciz "WEAP_TW_%c"
	.balign 4
	.asciz "WEP_STREAK"
	.balign 4
	.asciz "SUPERARROW"
	.balign 4
	.asciz "BOSSG_ELEC"
	.balign 4
	.asciz "BOSSG_ACID"
	.balign 4
	.asciz "HEAD_BREATHEF"
	.balign 4
	.asciz "HEAD_BREATHEE"
	.balign 4
	.asciz "HEAD_BREATHEA"
	.balign 4
	.asciz "FW_SHLD_ACTIVE"
	.balign 4
	.asciz "FAMILIAR1"
	.balign 4
	.asciz "FAMILIAR2"
	.balign 4
	.asciz "FAMILIAR_SPIT"
	.balign 4
	.asciz "PHOENIX_FBALL"
	.balign 4
	.asciz "InitPlayerMissiles failed."
	.balign 4
	.4byte 0

.global lbl_80111F70
lbl_80111F70:

	# ROM: 0x10EF70
	.asciz "S_ATK_QUICK"

.global lbl_80111F7C
lbl_80111F7C:

	# ROM: 0x10EF7C
	.asciz "S_ATK_SLOW"
	.balign 4

.global lbl_80111F88
lbl_80111F88:

	# ROM: 0x10EF88
	.asciz "S_DEFEND"
	.balign 4

.global lbl_80111F94
lbl_80111F94:

	# ROM: 0x10EF94
	.asciz "S_CHARGE"
	.balign 4

.global lbl_80111FA0
lbl_80111FA0:

	# ROM: 0x10EFA0
	.asciz "S_STRAFE"
	.balign 4

.global lbl_80111FAC
lbl_80111FAC:

	# ROM: 0x10EFAC
	.asciz "S_MAGIC_SHIELD"
	.balign 4

.global lbl_80111FBC
lbl_80111FBC:

	# ROM: 0x10EFBC
	.asciz "S_THROW_MAGIC"
	.balign 4

.global lbl_80111FCC
lbl_80111FCC:

	# ROM: 0x10EFCC
	.asciz "S_COMBO_MOVE"
	.balign 4

.global lbl_80111FDC
lbl_80111FDC:

	# ROM: 0x10EFDC
	.4byte 0x00000002
	.4byte 0x00000002
	.4byte 0x00000002
	.4byte 0x00000002
	.asciz "llo %2d lhi %2d rlo %2d rhi %2d tri %2d circle %2d x %2d  square %2d  \n"

.global lbl_80112034
lbl_80112034:

	# ROM: 0x10F034
	.asciz "Control %d has %s pressed.\n"

.global lbl_80112050
lbl_80112050:

	# ROM: 0x10F050
	.asciz "Error! Special Move %d stage = %d > %d"
	.balign 4

.global lbl_80112078
lbl_80112078:

	# ROM: 0x10F078
	.asciz "MTAP %d CONNECTED\n"
	.balign 4

.global lbl_8011208C
lbl_8011208C:

	# ROM: 0x10F08C
	.asciz "Disabling MTAP (load IRX failed)\n"
	.balign 4

.global lbl_801120B0
lbl_801120B0:

	# ROM: 0x10F0B0
	.asciz "Unable to open pad port %d %d\n"
	.balign 4

.global lbl_801120D0
lbl_801120D0:

	# ROM: 0x10F0D0
	.asciz "MTAP %d OPEN\n"
	.balign 4

.global lbl_801120E0
lbl_801120E0:

	# ROM: 0x10F0E0
	.asciz "SHADOW1L1"
	.balign 4

.global lbl_801120EC
lbl_801120EC:

	# ROM: 0x10F0EC
	.asciz "SHADOW2L1"
	.balign 4

.global lbl_801120F8
lbl_801120F8:

	# ROM: 0x10F0F8
	.asciz "SHADOW3L1"
	.balign 4

.global lbl_80112104
lbl_80112104:

	# ROM: 0x10F104
	.asciz "CRIT %s:%s HT:%d D:%d FR:%d TGT:%d DST:%d ANG:%d    "
	.balign 4

.global lbl_8011213C
lbl_8011213C:

	# ROM: 0x10F13C
	.asciz "CHLD %d %s:%s HT:%d D:d FR:%d TGT:%d DST:%d ANG:%d    "
	.balign 4

.global lbl_80112174
lbl_80112174:

	# ROM: 0x10F174
	.asciz "Critter has seq < 0 (shouldn't happen)"
	.balign 4

.global lbl_8011219C
lbl_8011219C:

	# ROM: 0x10F19C
	.asciz "Critter can not find move type: %d"
	.balign 4

.global lbl_801121C0
lbl_801121C0:

	# ROM: 0x10F1C0
	.asciz "Critter: > 1 killfx"

.global lbl_801121D4
lbl_801121D4:

	# ROM: 0x10F1D4
	.asciz "Critter unable to generate psys"

.global lbl_801121F4
lbl_801121F4:

	# ROM: 0x10F1F4
	.asciz "No Critter type %d subtype %d loaded"
	.balign 4

.global lbl_8011221C
lbl_8011221C:

	# ROM: 0x10F21C
	.asciz "Too many Critter Insts: %d"
	.balign 4

.global lbl_80112238
lbl_80112238:

	# ROM: 0x10F238
	.asciz "RED_FILLE"
	.balign 4

.global lbl_80112244
lbl_80112244:

	# ROM: 0x10F244
	.asciz "Too many Critter Anim Insts: %d"

.global lbl_80112264
lbl_80112264:

	# ROM: 0x10F264
	.asciz "Bad critter anim inst: %s"
	.balign 4
	.asciz "monsters/%s/%s"
	.balign 4
	.asciz "monsters/%s_%s"
	.balign 4
	.asciz "monsters/%s"

.global lbl_801122AC
lbl_801122AC:

	# ROM: 0x10F2AC
	.asciz "Critter can not find atree %s"
	.balign 4

.global lbl_801122CC
lbl_801122CC:

	# ROM: 0x10F2CC
	.asciz "Child critter defined before parent"

.global lbl_801122F0
lbl_801122F0:

	# ROM: 0x10F2F0
	.asciz "Critter %s: unable to find anim %s"
	.balign 4

.global lbl_80112314
lbl_80112314:

	# ROM: 0x10F314
	.asciz "Critter Header has no types"

.global lbl_80112330
lbl_80112330:

	# ROM: 0x10F330
	.asciz "CRITTER: AddAnim has addto idx %d > max %d"
	.balign 4
	.4byte 0

.global lbl_80112360
lbl_80112360:

	# ROM: 0x10F360
	.asciz "GRID ERROR"
	.balign 4
	.4byte 0

.global lbl_80112370
lbl_80112370:

	# ROM: 0x10F370
	.asciz "SCORPION"
	.balign 4

.global lbl_8011237C
lbl_8011237C:

	# ROM: 0x10F37C
	.asciz "SORCERER"
	.balign 4

.global lbl_80112388
lbl_80112388:

	# ROM: 0x10F388
	.asciz "LIZARDMAN"
	.balign 4

.global lbl_80112394
lbl_80112394:

	# ROM: 0x10F394
	.asciz "TREEFOLK"
	.balign 4

.global lbl_801123A0
lbl_801123A0:

	# ROM: 0x10F3A0
	.asciz "ICE GRUNT"
	.balign 4

.global lbl_801123AC
lbl_801123AC:

	# ROM: 0x10F3AC
	.asciz "ICE DEMON"
	.balign 4

.global lbl_801123B8
lbl_801123B8:

	# ROM: 0x10F3B8
	.asciz "SKELETON"
	.balign 4

.global lbl_801123C4
lbl_801123C4:

	# ROM: 0x10F3C4
	.asciz "WHIRLWIND"
	.balign 4

.global lbl_801123D0
lbl_801123D0:

	# ROM: 0x10F3D0
	.asciz "GARGOYLE"
	.balign 4

.global lbl_801123DC
lbl_801123DC:

	# ROM: 0x10F3DC
	.asciz "INACTIVE"
	.balign 4

.global lbl_801123E8
lbl_801123E8:

	# ROM: 0x10F3E8
	.asciz "ON_NEXT_LEVEL"
	.balign 4

.global lbl_801123F8
lbl_801123F8:

	# ROM: 0x10F3F8
	.asciz "DECORATION"
	.balign 4

.global lbl_80112404
lbl_80112404:

	# ROM: 0x10F404
	.asciz "CONTINUE"
	.balign 4

.global lbl_80112410
lbl_80112410:

	# ROM: 0x10F410
	.asciz "SHADOW1L1"
	.balign 4

.global lbl_8011241C
lbl_8011241C:

	# ROM: 0x10F41C
	.asciz "SHADOW2L1"
	.balign 4

.global lbl_80112428
lbl_80112428:

	# ROM: 0x10F428
	.asciz "SHADOW3L1"
	.balign 4
	.asciz "E%02X==PREV"
	.asciz "E%02X==NEXT"
	.asciz "E%02X: prev==next (%02X)"
	.balign 4

.global lbl_80112468
lbl_80112468:

	# ROM: 0x10F468
	.asciz "Enemy has non generator generator"
	.balign 4
	.asciz "%d > MAX:%d ETYPES\n"
	.asciz "monsters/%s/%s"
	.balign 4
	.asciz "monsters/%s_%s"
	.balign 4
	.asciz "monsters/%saux"
	.balign 4
	.asciz "monsters/%s%d"
	.balign 4
	.asciz "monsters/%s"

.global lbl_801124EC
lbl_801124EC:

	# ROM: 0x10F4EC
	.asciz "No enemy loaded: %s (type=%d subtype=%d)"
	.balign 4

.global lbl_80112518
lbl_80112518:

	# ROM: 0x10F518
	.asciz "Next Milestone Not Found (%02X)"

.global lbl_80112538
lbl_80112538:

	# ROM: 0x10F538
	.asciz "FINAL STATS"
	.asciz "GENERATORS"
	.balign 4
	.asciz "TREASURES"
	.balign 4
	.asciz "PLAYTIME"
	.balign 4
	.asciz "%3d:%02d:%02d"
	.balign 4
	.asciz "bosstats"
	.balign 4
	.asciz "TALBOARD%02d"
	.balign 4
	.asciz "Initializing Audio...\n"
	.balign 4
	.asciz "Loading Audio...\n"
	.balign 4
	.asciz "Loading Game.\n"
	.balign 4

.global lbl_801125D0
lbl_801125D0:

	# ROM: 0x10F5D0
	.asciz "TRANSITION_SCREEN"
	.balign 4

.global lbl_801125E4
lbl_801125E4:

	# ROM: 0x10F5E4
	.asciz "LoadTowerAndSelect Timeout"
	.balign 4

.global lbl_80112600
lbl_80112600:

	# ROM: 0x10F600
	.asciz "shopatt9"
	.balign 4
	.asciz "Starting wave %d... MEM=%d\n"
	.asciz "  Worlds... MEM=%d\n"
	.asciz "  Enemies... MEM=%d\n"
	.balign 4
	.asciz "  Items... MEM=%d\n"
	.balign 4
	.asciz "  Critters... MEM=%d\n"
	.balign 4
	.asciz "  Game... MEM=%d\n"
	.balign 4
	.asciz "  Done. MEM=%d\n"
	.asciz "AAAWHITE"
	.balign 4
	.asciz "FONT32_GLOW"
	.asciz "BUTTON_X"
	.balign 4
	.asciz "BUTTON_TRI"
	.balign 4
	.asciz "BUTTON_SQ"
	.balign 4
	.asciz "BUTTON_O"
	.balign 4
	.asciz "BUTTON_L"
	.balign 4
	.asciz "BUTTON_R"
	.balign 4
	.asciz "BUTTON_U"
	.balign 4
	.asciz "BUTTON_D"
	.balign 4
	.asciz "Audio Stop\n"
	.asciz "Reinit stuff\n"
	.balign 4
	.asciz "Reset Models\n"
	.balign 4
	.asciz "Init attract\n"
	.balign 4
	.asciz "TIMER_SAND"
	.balign 4
	.asciz "SAND_ANIM"
	.balign 4

.global lbl_80112770
lbl_80112770:

	# ROM: 0x10F770
	.asciz "THERMBASE"
	.balign 4

.global lbl_8011277C
lbl_8011277C:

	# ROM: 0x10F77C
	.asciz "THERMCOL"
	.balign 4

.global lbl_80112788
lbl_80112788:

	# ROM: 0x10F788
	.asciz "levels/level%s"
	.balign 4
	.asciz "dragon.wad"
	.balign 4
	.asciz "chimera.wad"
	.asciz "djinn.wad"
	.balign 4
	.asciz "drider.wad"
	.balign 4
	.asciz "pboss.wad"
	.balign 4
	.asciz "yeti.wad"
	.balign 4
	.asciz "lich.wad"
	.balign 4
	.asciz "wraith.wad"
	.balign 4
	.asciz "skorne1.wad"
	.asciz "skorne2.wad"
	.asciz "garm.wad"
	.balign 4
	.asciz "golemI.wad"
	.balign 4
	.asciz "golemF.wad"
	.balign 4
	.asciz "golem.wad"
	.balign 4
	.asciz "general.wad"
	.asciz "gar_%s.wad"
	.balign 4
	.asciz "C5XCNODE#0"
	.balign 4
	.asciz "Can't find WORLDOBJ: C5XCNODE#0"
	.asciz "C5XCNODE#1"
	.balign 4
	.asciz "Can't find WORLDOBJ: C5XCNODE#1"
	.asciz "PBOSSEYEBALL"
	.balign 4
	.asciz "A5XPXLNC_LNING"
	.balign 4
	.asciz "F2XPNC_LNING"
	.balign 4
	.asciz "Can't find WORLDOBJ: %s"
	.asciz "OTHER:         %8d\n"
	.asciz "WORLD (COL):   %8d\n"
	.asciz "WORLDNODES:    %8d\n"
	.asciz "WORLDOBJECTS:  %8d\n"
	.asciz "WORLDANIM:     %8d\n"
	.asciz "ITEMS:         %8d\n"
	.asciz "WEAPONS:       %8d\n"
	.asciz "ENEMIES:       %8d\n"
	.asciz "  %-8s %8d\n"
	.asciz "               --------\n"
	.balign 4
	.asciz "TOTAL:         %8d\n"

.global lbl_801129D4
lbl_801129D4:

	# ROM: 0x10F9D4
	.asciz "Enemy type %d has bad subtype %d"
	.balign 4

.global lbl_801129F8
lbl_801129F8:

	# ROM: 0x10F9F8
	.asciz "No valid worlds"
	.asciz "Worlddata with no worlds: %d"
	.balign 4
	.asciz "No world data: %s"
	.balign 4
	.asciz "No world data type %d"
	.balign 4
	.asciz "World Data %s has no cameras"
	.balign 4
	.asciz "World Data %s has no audio"
	.balign 4
	.asciz "S_%sNAME"
	.balign 4

.global lbl_80112A9C
lbl_80112A9C:

	# ROM: 0x10FA9C
	.asciz "No world data file: %s"
	.balign 4
	.4byte 0

.global lbl_80112AB8
lbl_80112AB8:

	# ROM: 0x10FAB8
	.asciz "objects.ngc"

.global lbl_80112AC4
lbl_80112AC4:

	# ROM: 0x10FAC4
	.asciz "RED_ARROWL1"

.global lbl_80112AD0
lbl_80112AD0:

	# ROM: 0x10FAD0
	.asciz "BLU_ARROWL1"

.global lbl_80112ADC
lbl_80112ADC:

	# ROM: 0x10FADC
	.asciz "YEL_ARROWL1"

.global lbl_80112AE8
lbl_80112AE8:

	# ROM: 0x10FAE8
	.asciz "GRE_ARROWL1"

.global lbl_80112AF4
lbl_80112AF4:

	# ROM: 0x10FAF4
	.4byte 0x0000012C
	.4byte 0x0000012C
	.4byte 0x0000012C

.global lbl_80112B00
lbl_80112B00:

	# ROM: 0x10FB00
	.asciz "ATTRACT MODE"
	.balign 4

.global lbl_80112B10
lbl_80112B10:

	# ROM: 0x10FB10
	.asciz "ENEMY SLOT FULL"

.global lbl_80112B20
lbl_80112B20:

	# ROM: 0x10FB20
	.asciz "NO SPACE AROUND"

.global lbl_80112B30
lbl_80112B30:

	# ROM: 0x10FB30
	.asciz "STOP TIME"
	.balign 4

.global lbl_80112B3C
lbl_80112B3C:

	# ROM: 0x10FB3C
	.asciz "NOT LOADED"
	.balign 4

.global lbl_80112B48
lbl_80112B48:

	# ROM: 0x10FB48
	.asciz "CONTAINER"
	.balign 4

.global lbl_80112B54
lbl_80112B54:

	# ROM: 0x10FB54
	.asciz "GENERATOR"
	.balign 4

.global lbl_80112B60
lbl_80112B60:

	# ROM: 0x10FB60
	.asciz "ENEMYINFO"
	.balign 4

.global lbl_80112B6C
lbl_80112B6C:

	# ROM: 0x10FB6C
	.asciz "OBSTICLE"
	.balign 4

.global lbl_80112B78
lbl_80112B78:

	# ROM: 0x10FB78
	.asciz "TRANSPORT"
	.balign 4

.global lbl_80112B84
lbl_80112B84:

	# ROM: 0x10FB84
	.asciz "RUNESTONE"
	.balign 4

.global lbl_80112B90
lbl_80112B90:

	# ROM: 0x10FB90
	.asciz "GEMSTONE"
	.balign 4

.global lbl_80112B9C
lbl_80112B9C:

	# ROM: 0x10FB9C
	.asciz "%s:%s:%d(%d)"
	.balign 4

.global lbl_80112BAC
lbl_80112BAC:

	# ROM: 0x10FBAC
	.asciz "%s:%s(%d)"
	.balign 4
	.asciz "EXIT_OFF"
	.balign 4
	.asciz "L1NSNC%c%d_ACTIVE"
	.balign 4
	.asciz "No Exit Obj: %s"
	.asciz "TREAS_GOLD"
	.balign 4
	.asciz "TREAS_SILVER"
	.balign 4
	.asciz "ITEMEXP0"
	.balign 4
	.asciz "TREAS_JUNK"
	.balign 4
	.asciz "CHESTSEXP0"
	.balign 4
	.asciz "CHESTGEXP0"
	.balign 4
	.asciz "GEN_SPECIAL%d"
	.balign 4
	.asciz "GEN_%s%d"
	.balign 4

.global lbl_80112C50
lbl_80112C50:

	# ROM: 0x10FC50
	.asciz "AllCoins"
	.balign 4

.global lbl_80112C5C
lbl_80112C5C:

	# ROM: 0x10FC5C
	.asciz "PINEAPPLE"
	.balign 4

.global lbl_80112C68
lbl_80112C68:

	# ROM: 0x10FC68
	.asciz "COL_OBJECT Item: idx < 0"
	.balign 4

.global lbl_80112C84
lbl_80112C84:

	# ROM: 0x10FC84
	.asciz "Special trigger has no target"
	.balign 4

.global lbl_80112CA4
lbl_80112CA4:

	# ROM: 0x10FCA4
	.asciz "Bad EnemyInfo: type %s subtype %d not loaded"
	.balign 4

.global lbl_80112CD4
lbl_80112CD4:

	# ROM: 0x10FCD4
	.asciz "EnemyInfo didn't generate %s(ai=%d, reason=%s)"
	.balign 4

.global lbl_80112D04
lbl_80112D04:

	# ROM: 0x10FD04
	.asciz "CAN'T FIND LOOKPUT PARAM:%d"

.global lbl_80112D20
lbl_80112D20:

	# ROM: 0x10FD20
	.asciz "Unable to AddItem '%s'"
	.balign 4

.global lbl_80112D38
lbl_80112D38:

	# ROM: 0x10FD38
	.asciz "G5BIGDIRT"
	.balign 4

.global lbl_80112D44
lbl_80112D44:

	# ROM: 0x10FD44
	.asciz "H4NSFFXL_PURPLE"

.global lbl_80112D54
lbl_80112D54:

	# ROM: 0x10FD54
	.asciz "NewItem: bad index"
	.balign 4

.global lbl_80112D68
lbl_80112D68:

	# ROM: 0x10FD68
	.asciz "Bad Item floor pos: '%s'  POS: %.0f %.0f %.0f"
	.balign 4

.global lbl_80112D98
lbl_80112D98:

	# ROM: 0x10FD98
	.asciz "> MAX ITEMS"
	.asciz "%d special triggers with id = %d"
	.balign 4
	.asciz "%d triggers with id = %d"
	.balign 4
	.asciz "Linked Triggers loop, id=%d->id=%d"
	.balign 4
	.asciz "Trigger id %d: no next %d"
	.balign 4

.global lbl_80112E24
lbl_80112E24:

	# ROM: 0x10FE24
	.asciz "Transporter id %d: no dest %d"
	.balign 4
	.asciz "Obelisk at %.2f %.2f %.2f"
	.balign 4
	.asciz "GAR_STATUE"
	.balign 4
	.asciz "GOL_STATUE"
	.balign 4
	.asciz "DEATHSTATUE1"
	.balign 4
	.asciz "DEATHSTATUE2"
	.balign 4
	.asciz "Item has wobjidx > max: %d"
	.balign 4
	.asciz "ITEM TARGET WOBJ %s: OBJECT NOT FOUND"
	.balign 4
	.asciz "Generator at %.1f %.1f %.1f has strength %d"

.global lbl_80112F08
lbl_80112F08:

	# ROM: 0x10FF08
	.asciz "SetItem: %s failed"
	.balign 4
	.asciz "TARGET WOBJ %s HAS NO NODE"
	.balign 4
	.asciz "ITEM WOBJ %s TRIGGER TYPES DIFFER: 0x%x != 0x%x"
	.asciz "TOO MANY ITEM WOBJS"
	.asciz "> MAX TRANSMITTERS"
	.balign 4
	.asciz "> MAX MILESTONES"
	.balign 4
	.asciz "> %d LOOKOUTS"
	.balign 4
	.asciz "Bad Locator (%d)"
	.balign 4

.global lbl_80112FC8
lbl_80112FC8:

	# ROM: 0x10FFC8
	.asciz "Two cameras link to trigger %d: %d and %d"
	.balign 4

.global lbl_80112FF4
lbl_80112FF4:

	# ROM: 0x10FFF4
	.asciz "DEATH_ICON"
	.balign 4

.global lbl_80113000
lbl_80113000:

	# ROM: 0x110000
	.asciz "powerups"
	.balign 4
	.asciz "items/%s"
	.balign 4
	.asciz "items/level%s"
	.balign 4

.global lbl_80113028
lbl_80113028:

	# ROM: 0x110028
	.asciz "soulsave"
	.balign 4

.global lbl_80113034
lbl_80113034:

	# ROM: 0x110034
	.asciz "goodbook"
	.balign 4

.global lbl_80113040
lbl_80113040:

	# ROM: 0x110040
	.asciz "scimitar"
	.balign 4

.global lbl_8011304C
lbl_8011304C:

	# ROM: 0x11004C
	.asciz "check.txt"
	.balign 4
	.asciz "Gauntlet"
	.balign 4
	.asciz "sony_screen00"
	.balign 4
	.asciz "sony_screen01"
	.balign 4
	.asciz "sony_screen02"
	.balign 4
	.asciz "sony_screen03"
	.balign 4
	.asciz "game_init_once (VERSION %s)\n"
	.balign 4
	.asciz "INITIALIZATION DONE. MEM=%d START=%08d END=%08d\n"
	.balign 4
	.asciz "Initializing the game...\n"
	.balign 4
	.asciz "Inital load done. MEM=%d\n"
	.balign 4
	.asciz "Initializing memory...\n"
	.asciz "Free memory %d K,  %d MB "
	.balign 4
	.asciz "LoadModel"
	.balign 4
	.asciz "Loading audio...\n"
	.balign 4
	.asciz "Initializing controls...\n"
	.balign 4
	.asciz "Initialization Done. MEM=%d\n"
	.balign 4

.global lbl_801131C0
lbl_801131C0:

	# ROM: 0x1101C0
	.asciz "Finish save cache transaction......\n"
	.balign 4

.global lbl_801131E8
lbl_801131E8:

	# ROM: 0x1101E8
	.asciz "Beginning save cache transaction......\n"
	.asciz "BASLUS-20047GameOpts"
	.balign 4
	.asciz "BASLUS-20047DirInfo"
	.asciz "BASLUS-20047save%04d"
	.balign 4
	.asciz "Entered SAVEMOUNT PORT/SLOT %d,%d \n"
	.asciz "SAVEMOUNT RETURNING 1....\n"
	.balign 4
	.asciz "SAVEMOUNT RETURNING -1....\n"
	.asciz "The device in Slot A is damaged and cannot be used.\nIf you decide to continue without saving,\nyou will not be able to save your game at a later stage.\n"
	.asciz "Another device in Slot A.\nPlease insert a Memory Card.\nIf you decide to continue without saving,\n you will not be able to save\nyour game at a later stage.\n"
	.asciz "Format Successful."
	.balign 4
	.asciz "The Memory Card in Slot A had an error."
	.asciz "MEMCARD.C"
	.balign 4
	.asciz "\"opening.bnr\" is not found in the root directory.\n"
	.balign 4
	.asciz "/carddemo/banner.tpl"
	.balign 4
	.asciz "/carddemo/icon.tpl"
	.balign 4
	.asciz "Gauntlet Save Data"
	.balign 4
	.asciz "The Gauntlet Dark Legacy Save File\non the Memory Card in Slot A is corrupt\nand must be deleted."
	.asciz "An error occurred when deleting the file.\nGame Save will be disabled. "
	.balign 4

.global lbl_80113548
lbl_80113548:

	# ROM: 0x110548
	.asciz "/opening.bnr"
	.balign 4
	.asciz "Unsupported banner texture formant."
	.asciz "Unsupported icon texture formant."
	.balign 4

.global lbl_801135A0
lbl_801135A0:

	# ROM: 0x1105A0
	.asciz "Press     Button when done."

.global lbl_801135BC
lbl_801135BC:

	# ROM: 0x1105BC
	.asciz "Scroll_A"
	.balign 4

.global lbl_801135C8
lbl_801135C8:

	# ROM: 0x1105C8
	.asciz "greenCircTrans"
	.balign 4

.global lbl_801135D8
lbl_801135D8:

	# ROM: 0x1105D8
	.asciz "greenCircTransM"

.global lbl_801135E8
lbl_801135E8:

	# ROM: 0x1105E8
	.asciz "window_empty"
	.balign 4

.global lbl_801135F8
lbl_801135F8:

	# ROM: 0x1105F8
	.asciz "orange_crystle"
	.balign 4

.global lbl_80113608
lbl_80113608:

	# ROM: 0x110608
	.asciz "red_crystle"

.global lbl_80113614
lbl_80113614:

	# ROM: 0x110614
	.asciz "purple_crystle"
	.balign 4

.global lbl_80113624
lbl_80113624:

	# ROM: 0x110624
	.asciz "cyan_crystle"
	.balign 4

.global lbl_80113634
lbl_80113634:

	# ROM: 0x110634
	.asciz "green_crystle"
	.balign 4

.global lbl_80113644
lbl_80113644:

	# ROM: 0x110644
	.asciz "yellow_crystle"
	.balign 4

.global lbl_80113654
lbl_80113654:

	# ROM: 0x110654
	.asciz "white_crystle"
	.balign 4

.global lbl_80113664
lbl_80113664:

	# ROM: 0x110664
	.asciz "black_crystle"
	.balign 4

.global lbl_80113674
lbl_80113674:

	# ROM: 0x110674
	.asciz "litch_piece"

.global lbl_80113680
lbl_80113680:

	# ROM: 0x110680
	.asciz "dragon_piece"
	.balign 4

.global lbl_80113690
lbl_80113690:

	# ROM: 0x110690
	.asciz "chimera_piece"
	.balign 4

.global lbl_801136A0
lbl_801136A0:

	# ROM: 0x1106A0
	.asciz "plague_piece"
	.balign 4

.global lbl_801136B0
lbl_801136B0:

	# ROM: 0x1106B0
	.asciz "dryder_piece"
	.balign 4

.global lbl_801136C0
lbl_801136C0:

	# ROM: 0x1106C0
	.asciz "genie_piece"

.global lbl_801136CC
lbl_801136CC:

	# ROM: 0x1106CC
	.asciz "yetti_piece"

.global lbl_801136D8
lbl_801136D8:

	# ROM: 0x1106D8
	.asciz "wraith_piece"
	.balign 4

.global lbl_801136E8
lbl_801136E8:

	# ROM: 0x1106E8
	.asciz "S1_border"
	.balign 4

.global lbl_801136F4
lbl_801136F4:

	# ROM: 0x1106F4
	.asciz "S2_border"
	.balign 4

.global lbl_80113700
lbl_80113700:

	# ROM: 0x110700
	.asciz "scimitar"
	.balign 4

.global lbl_8011370C
lbl_8011370C:

	# ROM: 0x11070C
	.asciz "soul_savior"

.global lbl_80113718
lbl_80113718:

	# ROM: 0x110718
	.asciz "fire_scroll"

.global lbl_80113724
lbl_80113724:

	# ROM: 0x110724
	.asciz "SAVER_AXE"
	.balign 4

.global lbl_80113730
lbl_80113730:

	# ROM: 0x110730
	.asciz "SAVER_HAM"
	.balign 4

.global lbl_8011373C
lbl_8011373C:

	# ROM: 0x11073C
	.asciz "SAVER_SWD"
	.balign 4

.global lbl_80113748
lbl_80113748:

	# ROM: 0x110748
	.asciz "SAVER_WND"
	.balign 4
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0

.global lbl_801137B4
lbl_801137B4:

	# ROM: 0x1107B4
	.asciz "Continue"
	.balign 4

.global lbl_801137C0
lbl_801137C0:

	# ROM: 0x1107C0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0

.global lbl_801137D0
lbl_801137D0:

	# ROM: 0x1107D0
	.asciz "CAM Y:%.0f P:%.0f D:%.2f(%.2f)  ATN:%.1f %.1f %.1f  "
	.balign 4

.global lbl_80113808
lbl_80113808:

	# ROM: 0x110808
	.asciz "level_data or level_data->camera NULL"
	.balign 4

.global lbl_80113830
lbl_80113830:

	# ROM: 0x110830
	.asciz "Music Volume"
	.balign 4

.global lbl_80113840
lbl_80113840:

	# ROM: 0x110840
	.asciz "Sfx Volume"
	.balign 4

.global lbl_8011384C
lbl_8011384C:

	# ROM: 0x11084C
	.asciz "Game Options"
	.balign 4

.global lbl_8011385C
lbl_8011385C:

	# ROM: 0x11085C
	.asciz "Controls"
	.balign 4

.global lbl_80113868
lbl_80113868:

	# ROM: 0x110868
	.asciz "Settings"
	.balign 4

.global lbl_80113874
lbl_80113874:

	# ROM: 0x110874
	.asciz "Manage Character"
	.balign 4

.global lbl_80113888
lbl_80113888:

	# ROM: 0x110888
	.asciz "Inventory"
	.balign 4

.global lbl_80113894
lbl_80113894:

	# ROM: 0x110894
	.asciz "Quit Game"
	.balign 4

.global lbl_801138A0
lbl_801138A0:

	# ROM: 0x1108A0
	.asciz "Tower Menu"
	.balign 4

.global lbl_801138AC
lbl_801138AC:

	# ROM: 0x1108AC
	.asciz "Quit Level"
	.balign 4

.global lbl_801138B8
lbl_801138B8:

	# ROM: 0x1108B8
	.asciz "Game Menu"
	.balign 4

.global lbl_801138C4
lbl_801138C4:

	# ROM: 0x1108C4
	.asciz "Difficulty"
	.balign 4

.global lbl_801138D0
lbl_801138D0:

	# ROM: 0x1108D0
	.asciz "Multiplayer Mode"
	.balign 4

.global lbl_801138E4
lbl_801138E4:

	# ROM: 0x1108E4
	.asciz "Abort Level?"
	.balign 4

.global lbl_801138F4
lbl_801138F4:

	# ROM: 0x1108F4
	.asciz "Quit Game?"
	.balign 4

.global lbl_80113900
lbl_80113900:

	# ROM: 0x110900
	.asciz "Shots Stun Other Players"
	.balign 4

.global lbl_8011391C
lbl_8011391C:

	# ROM: 0x11091C
	.asciz "Shots Hurt Other Players"
	.balign 4

.global lbl_80113938
lbl_80113938:

	# ROM: 0x110938
	.asciz "Rumble Feature "

.global lbl_80113948
lbl_80113948:

	# ROM: 0x110948
	.asciz "Auto Aim "
	.balign 4

.global lbl_80113954
lbl_80113954:

	# ROM: 0x110954
	.asciz "Auto Attack "
	.balign 4

.global lbl_80113964
lbl_80113964:

	# ROM: 0x110964
	.asciz "Control Style"
	.balign 4

.global lbl_80113974
lbl_80113974:

	# ROM: 0x110974
	.asciz "Rumble Feature"
	.balign 4

.global lbl_80113984
lbl_80113984:

	# ROM: 0x110984
	.asciz "Auto Aim"
	.balign 4

.global lbl_80113990
lbl_80113990:

	# ROM: 0x110990
	.asciz "Auto Attack"

.global lbl_8011399C
lbl_8011399C:

	# ROM: 0x11099C
	.asciz "General Hints"
	.balign 4

.global lbl_801139AC
lbl_801139AC:

	# ROM: 0x1109AC
	.asciz "Guardians"
	.balign 4

.global lbl_801139B8
lbl_801139B8:

	# ROM: 0x1109B8
	.asciz "Items of Legend"

.global lbl_801139C8
lbl_801139C8:

	# ROM: 0x1109C8
	.asciz "Runestones"
	.balign 4

.global lbl_801139D4
lbl_801139D4:

	# ROM: 0x1109D4
	.asciz "How Can I Help You?"

.global lbl_801139E8
lbl_801139E8:

	# ROM: 0x1109E8
	.asciz "CONTROLER_1"

.global lbl_801139F4
lbl_801139F4:

	# ROM: 0x1109F4
	.asciz "CONTROLER_2"

.global lbl_80113A00
lbl_80113A00:

	# ROM: 0x110A00
	.asciz "CONTROLER_3"

.global lbl_80113A0C
lbl_80113A0C:

	# ROM: 0x110A0C
	.asciz "A Hint for You"
	.balign 4

.global lbl_80113A1C
lbl_80113A1C:

	# ROM: 0x110A1C
	.asciz "AAAWHITE"
	.balign 4

.global lbl_80113A28
lbl_80113A28:

	# ROM: 0x110A28
	.asciz "FONT32_PARCH"
	.balign 4
	.asciz "empty_bar"
	.balign 4
	.asciz "pink_bar"
	.balign 4
	.asciz "marker_left"
	.asciz "marker_right"
	.balign 4

.global lbl_80113A6C
lbl_80113A6C:

	# ROM: 0x110A6C
	.asciz "Player %d"
	.balign 4

.global lbl_80113A78
lbl_80113A78:

	# ROM: 0x110A78
	.asciz "end_optmenu called with bad options_level"
	.balign 4

.global lbl_80113AA4
lbl_80113AA4:

	# ROM: 0x110AA4
	.asciz "OPTONS_MAXLEVEL EXCEEDED MAX"
	.balign 4

.global lbl_80113AC4
lbl_80113AC4:

	# ROM: 0x110AC4
	.asciz "FONT32GAR0"
	.balign 4

.global lbl_80113AD0
lbl_80113AD0:

	# ROM: 0x110AD0
	.asciz "ICON_ARROW"
	.balign 4
	.4byte 0

.global lbl_80113AE0
lbl_80113AE0:

	# ROM: 0x110AE0
	.asciz "trbo_full_new"
	.balign 4

.global lbl_80113AF0
lbl_80113AF0:

	# ROM: 0x110AF0
	.asciz "trbo_glint"
	.balign 4

.global lbl_80113AFC
lbl_80113AFC:

	# ROM: 0x110AFC
	.asciz "turbo_glow_new"
	.balign 4

.global lbl_80113B0C
lbl_80113B0C:

	# ROM: 0x110B0C
	.asciz "black_bar"
	.balign 4

.global lbl_80113B18
lbl_80113B18:

	# ROM: 0x110B18
	.asciz "trbo_gleem1"

.global lbl_80113B24
lbl_80113B24:

	# ROM: 0x110B24
	.asciz "POTION_ICON_RED"

.global lbl_80113B34
lbl_80113B34:

	# ROM: 0x110B34
	.asciz "POTION_ICON_BLU"

.global lbl_80113B44
lbl_80113B44:

	# ROM: 0x110B44
	.asciz "POTION_ICON_YEL"

.global lbl_80113B54
lbl_80113B54:

	# ROM: 0x110B54
	.asciz "POTION_ICON_GRE"

.global lbl_80113B64
lbl_80113B64:

	# ROM: 0x110B64
	.asciz "Levitation"
	.balign 4

.global lbl_80113B70
lbl_80113B70:

	# ROM: 0x110B70
	.asciz "Invisible"
	.balign 4

.global lbl_80113B7C
lbl_80113B7C:

	# ROM: 0x110B7C
	.asciz "StopTime"
	.balign 4

.global lbl_80113B88
lbl_80113B88:

	# ROM: 0x110B88
	.asciz "FireBreath"
	.balign 4

.global lbl_80113B94
lbl_80113B94:

	# ROM: 0x110B94
	.asciz "AcidBreath"
	.balign 4

.global lbl_80113BA0
lbl_80113BA0:

	# ROM: 0x110BA0
	.asciz "LightningBreath"

.global lbl_80113BB0
lbl_80113BB0:

	# ROM: 0x110BB0
	.asciz "EnemyShrink"

.global lbl_80113BBC
lbl_80113BBC:

	# ROM: 0x110BBC
	.asciz "BossHorns"
	.balign 4

.global lbl_80113BC8
lbl_80113BC8:

	# ROM: 0x110BC8
	.asciz "BossMask"
	.balign 4

.global lbl_80113BD4
lbl_80113BD4:

	# ROM: 0x110BD4
	.asciz "BossGauntR"
	.balign 4

.global lbl_80113BE0
lbl_80113BE0:

	# ROM: 0x110BE0
	.asciz "BossGauntL"
	.balign 4

.global lbl_80113BEC
lbl_80113BEC:

	# ROM: 0x110BEC
	.asciz "HandOfDeath"

.global lbl_80113BF8
lbl_80113BF8:

	# ROM: 0x110BF8
	.asciz "HealthVamp"
	.balign 4

.global lbl_80113C04
lbl_80113C04:

	# ROM: 0x110C04
	.asciz "Fire Shield"

.global lbl_80113C10
lbl_80113C10:

	# ROM: 0x110C10
	.asciz "Elec Shield"

.global lbl_80113C1C
lbl_80113C1C:

	# ROM: 0x110C1C
	.asciz "Resist Light"
	.balign 4

.global lbl_80113C2C
lbl_80113C2C:

	# ROM: 0x110C2C
	.asciz "Gas Mask"
	.balign 4

.global lbl_80113C38
lbl_80113C38:

	# ROM: 0x110C38
	.asciz "Resist Magic"
	.balign 4

.global lbl_80113C48
lbl_80113C48:

	# ROM: 0x110C48
	.asciz "Immune Fire"

.global lbl_80113C54
lbl_80113C54:

	# ROM: 0x110C54
	.asciz "Immune Elec"

.global lbl_80113C60
lbl_80113C60:

	# ROM: 0x110C60
	.asciz "Immune Light"
	.balign 4

.global lbl_80113C70
lbl_80113C70:

	# ROM: 0x110C70
	.asciz "Immune Acid"

.global lbl_80113C7C
lbl_80113C7C:

	# ROM: 0x110C7C
	.asciz "Immune Magic"
	.balign 4

.global lbl_80113C8C
lbl_80113C8C:

	# ROM: 0x110C8C
	.asciz "Immune Gas"
	.balign 4

.global lbl_80113C98
lbl_80113C98:

	# ROM: 0x110C98
	.asciz "Invulnerable"
	.balign 4

.global lbl_80113CA8
lbl_80113CA8:

	# ROM: 0x110CA8
	.asciz "Reflective Armor"
	.balign 4

.global lbl_80113CBC
lbl_80113CBC:

	# ROM: 0x110CBC
	.asciz "Knockback Armor"

.global lbl_80113CCC
lbl_80113CCC:

	# ROM: 0x110CCC
	.asciz "AntiDeath"
	.balign 4

.global lbl_80113CD8
lbl_80113CD8:

	# ROM: 0x110CD8
	.asciz "Gold Invuln"

.global lbl_80113CE4
lbl_80113CE4:

	# ROM: 0x110CE4
	.asciz "Fire Armor"
	.balign 4

.global lbl_80113CF0
lbl_80113CF0:

	# ROM: 0x110CF0
	.asciz "Elec Armor"
	.balign 4

.global lbl_80113CFC
lbl_80113CFC:

	# ROM: 0x110CFC
	.asciz "Armor Protect"
	.balign 4

.global lbl_80113D0C
lbl_80113D0C:

	# ROM: 0x110D0C
	.asciz "Armor Reflect"
	.balign 4

.global lbl_80113D1C
lbl_80113D1C:

	# ROM: 0x110D1C
	.asciz "KnockBack"
	.balign 4

.global lbl_80113D28
lbl_80113D28:

	# ROM: 0x110D28
	.asciz "KnockDown"
	.balign 4

.global lbl_80113D34
lbl_80113D34:

	# ROM: 0x110D34
	.asciz "BlownAway"
	.balign 4

.global lbl_80113D40
lbl_80113D40:

	# ROM: 0x110D40
	.asciz "KnockOver"
	.balign 4

.global lbl_80113D4C
lbl_80113D4C:

	# ROM: 0x110D4C
	.asciz "PoisonGas"
	.balign 4

.global lbl_80113D58
lbl_80113D58:

	# ROM: 0x110D58
	.asciz "DeathStun"
	.balign 4

.global lbl_80113D64
lbl_80113D64:

	# ROM: 0x110D64
	.asciz "Whirlwind"
	.balign 4

.global lbl_80113D70
lbl_80113D70:

	# ROM: 0x110D70
	.asciz "FireBall"
	.balign 4

.global lbl_80113D7C
lbl_80113D7C:

	# ROM: 0x110D7C
	.asciz "3Way Shot"
	.balign 4

.global lbl_80113D88
lbl_80113D88:

	# ROM: 0x110D88
	.asciz "Super Shot"
	.balign 4

.global lbl_80113D94
lbl_80113D94:

	# ROM: 0x110D94
	.asciz "Weapon Reflect"
	.balign 4

.global lbl_80113DA4
lbl_80113DA4:

	# ROM: 0x110DA4
	.asciz "5Way Shot"
	.balign 4

.global lbl_80113DB0
lbl_80113DB0:

	# ROM: 0x110DB0
	.asciz "Weapon Heal"

.global lbl_80113DBC
lbl_80113DBC:

	# ROM: 0x110DBC
	.asciz "Weapon Turbo"
	.balign 4

.global lbl_80113DCC
lbl_80113DCC:

	# ROM: 0x110DCC
	.asciz "Weapon Sticky"
	.balign 4

.global lbl_80113DDC
lbl_80113DDC:

	# ROM: 0x110DDC
	.asciz "RapidFire"
	.balign 4

.global lbl_80113DE8
lbl_80113DE8:

	# ROM: 0x110DE8
	.asciz "Weapon Low"
	.balign 4

.global lbl_80113DF4
lbl_80113DF4:

	# ROM: 0x110DF4
	.asciz "LEFTHAND"
	.balign 4

.global lbl_80113E00
lbl_80113E00:

	# ROM: 0x110E00
	.asciz "RIGHTHAN"
	.balign 4
	.asciz "MIKEYPUP"
	.balign 4
	.4byte 0x00000001
	.4byte 0x0000000F
	.4byte 0x00000025
	.4byte 0x00000033

.global lbl_80113E28
lbl_80113E28:

	# ROM: 0x110E28
	.asciz "16_%sCOIN"
	.balign 4

.global lbl_80113E34
lbl_80113E34:

	# ROM: 0x110E34
	.asciz "SM_CRYSTAL_%s"
	.balign 4
	.asciz "Wait In Tower"
	.balign 4
	.asciz "Quit Game"
	.balign 4
	.asciz "IN TOWER"
	.balign 4
	.asciz "NO FLOOR"
	.balign 4
	.asciz "%.1Lf %.1Lf %.1Lf"
	.balign 4
	.asciz "%.0Lf %.0Lf"
	.asciz "S4_FRAME"
	.balign 4
	.asciz "BK_RUNE_STONE_02"
	.balign 4
	.asciz "QUEST_ICON"
	.balign 4
	.asciz "DemoWelcome"
	.asciz "DemoLevel"
	.balign 4
	.asciz "WelcomeMessage"
	.balign 4
	.asciz "GarmMessage"
	.asciz "TRANSMITTER: CT=%02X, C1=%02X, C2=%02X, RATIO=%4.2f"

.global lbl_80113F2C
lbl_80113F2C:

	# ROM: 0x110F2C
	.asciz "mikey_objgrp OBJ NODE HAS KIDS AFTER ATREEDELETE"
	.balign 4

.global lbl_80113F60
lbl_80113F60:

	# ROM: 0x110F60
	.asciz "PLAYER OBJ NODE HAS KIDS AFTER ATREEDELETE"
	.balign 4

.global lbl_80113F8C
lbl_80113F8C:

	# ROM: 0x110F8C
	.asciz "Game load failed !!"

.global lbl_80113FA0
lbl_80113FA0:

	# ROM: 0x110FA0
	.asciz "Game save failed !!"
	.asciz "PLAYER OBJ NODE EXISTS BEFORE LOAD_PLAYER"
	.balign 4
	.asciz "Player Atree %s not found"
	.balign 4
	.asciz "%sCFGLOW"
	.balign 4
	.asciz "WEAP_HOLD"
	.balign 4
	.asciz "WEAP_%s_HD%d"
	.balign 4
	.asciz "SHADOWL1"
	.balign 4
	.asciz "Unlimited?"
	.balign 4
	.asciz "NoDamage?"
	.balign 4
	.asciz "Select a character ?"
	.balign 4
	.asciz "Worlds ?"
	.balign 4
	.asciz "Mike and Bob say,\nThank You for Playing!!"
	.balign 4

.global lbl_80114098
lbl_80114098:

	# ROM: 0x111098
	.asciz "players/%s/sfx%s"
	.balign 4
	.asciz "players/%s/%s"
	.balign 4
	.asciz "players/%s/%s%d0"
	.balign 4
	.asciz "players/%s/anim"

.global lbl_801140E0
lbl_801140E0:

	# ROM: 0x1110E0
	.asciz "KEY_ICON"
	.balign 4
	.asciz "SM_RUNE_%s_%02d"
	.asciz "SM_KEY_%s"
	.balign 4
	.asciz "BTMBK_LEVL"
	.balign 4

.global lbl_80114114
lbl_80114114:

	# ROM: 0x111114
	.asciz "    Player %d... MEM=%d\n"
	.balign 4

.global lbl_80114130
lbl_80114130:

	# ROM: 0x111130
	.asciz "PLAYER %d"
	.balign 4
	.asciz "BOSSGAUNTL"
	.balign 4
	.asciz "HEAD_HANDOFDEATH"
	.balign 4
	.asciz "HEAD_HEALTHVAMP"
	.asciz "BOSSHORNS"
	.balign 4
	.asciz "BOSSMASK"
	.balign 4
	.asciz "HEAD_HALO"
	.balign 4
	.asciz "HEAD_GAS"
	.balign 4
	.asciz "HEAD_XRAY"
	.balign 4
	.asciz "BOSSGAUNTR"
	.balign 4
	.asciz "SUPERXBOW"
	.balign 4
	.asciz "HAMMER_HD"
	.balign 4

.global lbl_801141CC
lbl_801141CC:

	# ROM: 0x1111CC
	.asciz "MIKEYPUP_ON"
	.asciz "KEY_RING"
	.balign 4
	.asciz "BADFRUIT"
	.balign 4
	.asciz "SPECIALS"
	.balign 4
	.asciz "RUNESTONE"
	.balign 4
	.asciz "GOLDNICON"
	.balign 4

.global lbl_80114214
lbl_80114214:

	# ROM: 0x111214
	.asciz "INVENTORY"
	.balign 4

.global lbl_80114220
lbl_80114220:

	# ROM: 0x111220
	.asciz "Can't find start near other player"
	.balign 4
	.asciz "Bad Player Matrix"
	.balign 4
	.asciz "CHIMKEY_PART"
	.balign 4
	.asciz "PARTICLE1_A"
	.asciz "POJOBODY1_HE#1"
	.balign 4
	.4byte 0

.global lbl_80114288
lbl_80114288:

	# ROM: 0x111288
	.asciz "Unable to generate psys"

.global lbl_801142A0
lbl_801142A0:

	# ROM: 0x1112A0
	.asciz "pdata file %s: file on disk go too large for buffer"

.global lbl_801142D4
lbl_801142D4:

	# ROM: 0x1112D4
	.asciz "No player data file: %s"
	.4byte 0

.global lbl_801142F0
lbl_801142F0:

	# ROM: 0x1112F0
	.asciz "REBOOT IOP (%s%s)"
	.balign 4
	.asciz "cdrom0:\\\\"
	.balign 4
	.asciz "IOPRP21.IMG"
	.asciz "cdrom0:\\\\IOPRP21.IMG"
	.balign 4
	.asciz "sceSifRebootIop %s%s Failed: %d"
	.asciz "cdrom0:\\\\IRX\\"
	.balign 4
	.asciz "mem2MB.irx"
	.balign 4
	.asciz "LOAD IRX (%s)"
	.balign 4
	.asciz "sio2man.irx"
	.asciz "mtapman.irx"
	.asciz "mcman.irx"
	.balign 4
	.asciz "mcserv.irx"
	.balign 4
	.asciz "padman.irx"
	.balign 4
	.asciz "libsd.irx"
	.balign 4
	.asciz "Can't load module: %s (%d) (%s)\n"
	.balign 4
	.asciz "%s(%d)(%s)\n"

.global lbl_801143F8
lbl_801143F8:

	# ROM: 0x1113F8
	.asciz "selscrn_strength"
	.balign 4

.global lbl_8011440C
lbl_8011440C:

	# ROM: 0x11140C
	.asciz "selscrn_armor"
	.balign 4

.global lbl_8011441C
lbl_8011441C:

	# ROM: 0x11141C
	.asciz "selscrn_magic"
	.balign 4

.global lbl_8011442C
lbl_8011442C:

	# ROM: 0x11142C
	.asciz "selscrn_speed"
	.balign 4

.global lbl_8011443C
lbl_8011443C:

	# ROM: 0x11143C
	.asciz "STRENGTH"
	.balign 4
	.4byte lbl_8011443C
	.4byte lbl_80347E90
	.4byte lbl_80347E98
	.4byte lbl_80347EA0

.global lbl_80114458
lbl_80114458:

	# ROM: 0x111458
	.asciz "VALKYRIE"
	.balign 4

.global lbl_80114464
lbl_80114464:

	# ROM: 0x111464
	.asciz "SORCERESS"
	.balign 4

.global lbl_80114470
lbl_80114470:

	# ROM: 0x111470
	.asciz "MINOTAUR"
	.balign 4

.global lbl_8011447C
lbl_8011447C:

	# ROM: 0x11147C
	.asciz "FALCONESS"
	.balign 4
	.asciz "Loading..."
	.balign 4
	.asciz "S12_%s_%s"
	.balign 4

.global lbl_801144A0
lbl_801144A0:

	# ROM: 0x1114A0
	.asciz "S12_WEAP_%s"
	.asciz "Press START"
	.asciz "to Save/Load"
	.balign 4
	.asciz "or Change"
	.balign 4
	.asciz "Character"
	.balign 4
	.asciz "SUM_NAME"
	.balign 4
	.asciz "Not Saved."
	.balign 4
	.asciz "Load new"
	.balign 4
	.asciz "in Slot A"
	.balign 4
	.asciz "will now be"
	.asciz "accessed."
	.balign 4
	.asciz "Do not touch"
	.balign 4
	.asciz "POWER Button"
	.balign 4
	.asciz "Memory Card"
	.asciz "No Gauntlet"
	.asciz "save data on"
	.balign 4
	.asciz "Choose a"
	.balign 4
	.asciz "File to Load"
	.balign 4
	.asciz "Location to"
	.asciz "Write to"
	.balign 4
	.asciz "Unformatted"
	.asciz "Would you"
	.balign 4
	.asciz "format it?"
	.balign 4
	.asciz "Formatting..."
	.balign 4
	.asciz "the POWER"
	.balign 4
	.asciz "Memory Card!"
	.balign 4
	.asciz "Complete."
	.balign 4
	.asciz "Format Memory"
	.balign 4
	.asciz "And Create"
	.balign 4
	.asciz "%d Blocks"
	.balign 4
	.asciz "Save File?"
	.balign 4
	.asciz "%d Blocks Free"
	.balign 4
	.asciz "Creating"
	.balign 4
	.asciz "Gauntlet"
	.balign 4
	.asciz "Save File..."
	.balign 4
	.asciz "Insufficient"
	.balign 4
	.asciz "Free Space."
	.asciz "Save File"
	.balign 4
	.asciz "Created."
	.balign 4
	.asciz "Overwrite"
	.balign 4
	.asciz "Existing"
	.balign 4
	.asciz "Writing to"
	.balign 4
	.asciz "Loading from"
	.balign 4
	.asciz "Complete"
	.balign 4
	.asciz "Are you sure"
	.balign 4
	.asciz "you want"
	.balign 4
	.asciz "to Quit?"
	.balign 4
	.asciz "Quit Anyway?"
	.balign 4

.global lbl_80114718
lbl_80114718:

	# ROM: 0x111718
	.asciz " SLOT  %d%c"

.global lbl_80114724
lbl_80114724:

	# ROM: 0x111724
	.asciz " SLOT  %d"
	.balign 4
	.asciz "ATT_GLOW"
	.balign 4
	.asciz "Level %d"
	.balign 4
	.asciz "selscrn_questmark"
	.balign 4
	.asciz "S1_plyr%d"
	.balign 4
	.asciz "S2_plyr%d"
	.balign 4
	.asciz "S1_border"
	.balign 4
	.asciz "S2_border"
	.balign 4
	.4byte 0

.global lbl_80114790
lbl_80114790:

	# ROM: 0x111790
	.asciz "Bad Effect type: %d"
	.asciz "SEETHROUGH"
	.balign 4
	.asciz "PBOSSQEYEBALL"
	.balign 4
	.asciz "Effect %s not found"
	.asciz "SUICIDEEXP"
	.balign 4
	.asciz "DEATHFX1"
	.balign 4
	.asciz "DEATHFX2"
	.balign 4
	.asciz "DEATH_ARC"
	.balign 4
	.asciz "DEATH_EXP"
	.balign 4
	.asciz "LEGENDFX"
	.balign 4
	.asciz "LEGENDFX2"
	.balign 4
	.asciz "LEGENDPRJ"
	.balign 4
	.asciz "LEGENDHLD"
	.balign 4
	.asciz "SAFEREXP"
	.balign 4
	.asciz "WORLD_EXP"
	.balign 4
	.asciz "AAAWHITE"
	.balign 4
	.asciz "CHROMESILVER"
	.balign 4
	.asciz "CHROMEGOLD"
	.balign 4
	.asciz "DEATHLIGHT"
	.balign 4
	.asciz "DEATHMAGIC"
	.balign 4
	.asciz "DEATHBLOOD"
	.balign 4
	.asciz "DEATHFIRE"
	.balign 4
	.asciz "DEATHELEC"
	.balign 4
	.asciz "DEATHACID"
	.balign 4
	.asciz "DEATHGOLEM"
	.balign 4
	.asciz "DEATHALT"
	.balign 4

.global lbl_801148E0
lbl_801148E0:

	# ROM: 0x1118E0
	.asciz "Unable to find effect '%s'"
	.balign 4

.global lbl_801148FC
lbl_801148FC:

	# ROM: 0x1118FC
	.asciz "> Max = %d custon effects"
	.balign 4

.global lbl_80114918
lbl_80114918:

	# ROM: 0x111918
	.asciz "SHP_GOLD"
	.balign 4

.global lbl_80114924
lbl_80114924:

	# ROM: 0x111924
	.asciz "SHP_BONES"
	.balign 4

.global lbl_80114930
lbl_80114930:

	# ROM: 0x111930
	.asciz "SHOP_SCROLL_1"
	.balign 4

.global lbl_80114940
lbl_80114940:

	# ROM: 0x111940
	.asciz "SHOP_SCROLL_2"
	.balign 4

.global lbl_80114950
lbl_80114950:

	# ROM: 0x111950
	.asciz "S2_BORDER"
	.balign 4

.global lbl_8011495C
lbl_8011495C:

	# ROM: 0x11195C
	.asciz "Loading..."
	.balign 4
	.asciz "Final Stats"
	.asciz "Enemies Killed"
	.balign 4
	.asciz "Generators"
	.balign 4
	.asciz "Destroyed"
	.balign 4
	.asciz "Gold Found"
	.balign 4
	.asciz "Total Playtime"
	.balign 4
	.asciz "%d Hours"
	.balign 4
	.asciz "%d Minutes"
	.balign 4
	.asciz "Continue"
	.balign 4
	.asciz "Level %d"
	.balign 4
	.asciz "Strength"
	.balign 4

.global lbl_801149F4
lbl_801149F4:

	# ROM: 0x1119F4
	.asciz " Continue"
	.balign 4
	.asciz "S1_PLYR%d"
	.balign 4
	.asciz "S2_PLYR%d"
	.balign 4
	.asciz "S1_BORDER"
	.balign 4
	.asciz "SHOP_TOP_%s"

.global lbl_80114A30
lbl_80114A30:

	# ROM: 0x111A30
	.asciz "MORE_DOWN"
	.balign 4

.global lbl_80114A3C
lbl_80114A3C:

	# ROM: 0x111A3C
	.asciz "shop.wad"
	.balign 4

.global lbl_80114A48
lbl_80114A48:

	# ROM: 0x111A48
	.asciz "wizmus.s"
	.balign 4

.global lbl_80114A54
lbl_80114A54:

	# ROM: 0x111A54
	.asciz "valmus.s"
	.balign 4

.global lbl_80114A60
lbl_80114A60:

	# ROM: 0x111A60
	.asciz "warmus.s"
	.balign 4

.global lbl_80114A6C
lbl_80114A6C:

	# ROM: 0x111A6C
	.asciz "arcmus.s"
	.balign 4

.global lbl_80114A78
lbl_80114A78:

	# ROM: 0x111A78
	.asciz "grunt_a.s"
	.balign 4

.global lbl_80114A84
lbl_80114A84:

	# ROM: 0x111A84
	.asciz "grunt_b.s"
	.balign 4

.global lbl_80114A90
lbl_80114A90:

	# ROM: 0x111A90
	.asciz "grunt_c.s"
	.balign 4

.global lbl_80114A9C
lbl_80114A9C:

	# ROM: 0x111A9C
	.asciz "grunt_d.s"
	.balign 4

.global lbl_80114AA8
lbl_80114AA8:

	# ROM: 0x111AA8
	.asciz "demon_b.s"
	.balign 4

.global lbl_80114AB4
lbl_80114AB4:

	# ROM: 0x111AB4
	.asciz "demon_c.s"
	.balign 4

.global lbl_80114AC0
lbl_80114AC0:

	# ROM: 0x111AC0
	.asciz "demon_d.s"
	.balign 4

.global lbl_80114ACC
lbl_80114ACC:

	# ROM: 0x111ACC
	.asciz "3dfxsplash.s"
	.balign 4

.global lbl_80114ADC
lbl_80114ADC:

	# ROM: 0x111ADC
	.asciz "kata-sor.s"
	.balign 4

.global lbl_80114AE8
lbl_80114AE8:

	# ROM: 0x111AE8
	.asciz "kata-kni.s"
	.balign 4

.global lbl_80114AF4
lbl_80114AF4:

	# ROM: 0x111AF4
	.asciz "kata-dwf.s"
	.balign 4

.global lbl_80114B00
lbl_80114B00:

	# ROM: 0x111B00
	.asciz "kata-jes.s"
	.balign 4

.global lbl_80114B0C
lbl_80114B0C:

	# ROM: 0x111B0C
	.asciz "grunt_g.s"
	.balign 4

.global lbl_80114B18
lbl_80114B18:

	# ROM: 0x111B18
	.asciz "demon_k.s"
	.balign 4

.global lbl_80114B24
lbl_80114B24:

	# ROM: 0x111B24
	.asciz "grunt_i.s"
	.balign 4

.global lbl_80114B30
lbl_80114B30:

	# ROM: 0x111B30
	.asciz "demon_i.s"
	.balign 4

.global lbl_80114B3C
lbl_80114B3C:

	# ROM: 0x111B3C
	.asciz "grunt_j.s"
	.balign 4

.global lbl_80114B48
lbl_80114B48:

	# ROM: 0x111B48
	.asciz "grunt_h.s"
	.balign 4

.global lbl_80114B54
lbl_80114B54:

	# ROM: 0x111B54
	.asciz "attr-newgame.s"
	.balign 4

.global lbl_80114B64
lbl_80114B64:

	# ROM: 0x111B64
	.asciz "attr-playme.s"
	.balign 4

.global lbl_80114B74
lbl_80114B74:

	# ROM: 0x111B74
	.asciz "gamegiveaway.s"
	.balign 4

.global lbl_80114B84
lbl_80114B84:

	# ROM: 0x111B84
	.asciz "skorne2garm.s"
	.balign 4

.global lbl_80114B94
lbl_80114B94:

	# ROM: 0x111B94
	.asciz "completion.s"
	.balign 4
	.asciz "S_GOL%cSTOMP"
	.balign 4
	.asciz "S_GOL%cBORN"
	.asciz "S_GOL%cSWING"
	.balign 4
	.asciz "S_%s%sCLOSE"
	.asciz "S_%s%sFAR"
	.balign 4
	.asciz "S_%sHITCLOSE"
	.balign 4
	.asciz "S_%sHITFAR"
	.balign 4
	.asciz "S_%sHIT1CLOSE"
	.balign 4
	.asciz "S_%sHIT2CLOSE"
	.balign 4
	.asciz "S_%sHIT1FAR"
	.asciz "S_%sHIT2FAR"
	.asciz "S_%sSTRIKE"
	.balign 4
	.asciz "S_%sBITE"
	.balign 4

.global lbl_80114C54
lbl_80114C54:

	# ROM: 0x111C54
	.asciz "S_%s%s1S"
	.balign 4

.global lbl_80114C60
lbl_80114C60:

	# ROM: 0x111C60
	.asciz "S_%s%s2S"
	.balign 4

.global lbl_80114C6C
lbl_80114C6C:

	# ROM: 0x111C6C
	.asciz "ITEM-SOUND 0x%08x TID %d **BAD** TRACK %08x\n"
	.balign 4

.global lbl_80114C9C
lbl_80114C9C:

	# ROM: 0x111C9C
	.asciz "S_SHOP_%c"
	.balign 4

.global lbl_80114CA8
lbl_80114CA8:

	# ROM: 0x111CA8
	.asciz "S_MAP_%c"
	.balign 4

.global lbl_80114CB4
lbl_80114CB4:

	# ROM: 0x111CB4
	.asciz "PLAYER%d"
	.balign 4

.global lbl_80114CC0
lbl_80114CC0:

	# ROM: 0x111CC0
	.asciz "Audio stream does not exist: %s"

.global lbl_80114CE0
lbl_80114CE0:

	# ROM: 0x111CE0
	.asciz "Audio stream won't play: %s"
	.asciz "S_%sROTATE"
	.balign 4
	.asciz "S_ELV%s%cB"
	.balign 4
	.asciz "S_ELV%s%c"
	.balign 4
	.asciz "S_%sSTOP"
	.balign 4
	.asciz "S_ELV%sSTP%cB"
	.balign 4
	.asciz "S_ELV%sSTP%c"
	.balign 4
	.4byte 0

.global lbl_80114D50
lbl_80114D50:

	# ROM: 0x111D50
	.asciz "NeedGargItems"
	.balign 4

.global lbl_80114D60
lbl_80114D60:

	# ROM: 0x111D60
	.asciz "DemoClosed"
	.balign 4

.global lbl_80114D6C
lbl_80114D6C:

	# ROM: 0x111D6C
	.asciz "NeedCrystals"
	.balign 4
	.asciz "L1XPLIGHTRAY01"
	.balign 4
	.asciz "UnlockSection"
	.balign 4
	.asciz "UnlockLevel"
	.asciz "L1WINDOWFRAME"
	.balign 4
	.asciz "No object: WINDOWFRAME"
	.balign 4
	.asciz "L1RUNEPLACE"
	.asciz "No object: RUNEPLACE"
	.balign 4
	.asciz "L1RUNE13"
	.balign 4
	.asciz "No object: RUNE13"
	.balign 4
	.asciz "NewRunes"
	.balign 4
	.asciz "Rune13No"
	.balign 4
	.asciz "All12RunesNo"
	.balign 4
	.asciz "All12RunesYes"
	.balign 4
	.asciz "Rune13Yes"
	.balign 4
	.asciz "NewShards"
	.balign 4
	.asciz "MoreShards"
	.balign 4
	.asciz "AllShards"
	.balign 4
	.4byte 0

.global lbl_80114E80
lbl_80114E80:

	# ROM: 0x111E80
	.asciz "SCROLL_A"
	.balign 4
	.4byte 0

.global lbl_80114E90
lbl_80114E90:

	# ROM: 0x111E90
	.asciz "CLAMP ST"
	.balign 4
	.asciz "AUDIO VIEW"
	.balign 4
	.asciz "MODE    PART/BANK      Sound NAME         NUM       LENGTH"
	.balign 4
	.asciz "VOL:%03d"
	.balign 4
	.asciz "PAN:%03d"
	.balign 4
	.asciz "MASTERVOL:%03d"
	.balign 4
	.asciz "TESTMODE:%2d  "
	.balign 4
	.asciz "TRACK:%s"
	.balign 4
	.asciz "PRI:%03d"
	.balign 4
	.asciz "ACK: 0x%04x "
	.balign 4
	.asciz "                    "
	.balign 4
	.asciz "%-18s 0x%06x  %05.2Lf  "
	.asciz "                                     "
	.balign 4
	.asciz "AAANULLOBJ"
	.balign 4

.global lbl_80114FA8
lbl_80114FA8:

	# ROM: 0x111FA8
	.asciz "AAAWHITE"
	.balign 4
	.asciz "ATREE %s\n"
	.balign 4
	.asciz "FRAME: %02.1Lf MAX: %02d RATE:%02d  "
	.balign 4
	.asciz "POS: %.2Lf %.2Lf %.2Lf  PYR: %.2Lf %.2Lf %.2Lf   "
	.balign 4

.global lbl_8011501C
lbl_8011501C:

	# ROM: 0x11201C
	.asciz "VIEWING ALHA CHANNEL"
	.balign 4
	.asciz "ABGR_1555"
	.balign 4
	.asciz "_BGR__555"
	.balign 4
	.asciz "ABGR_8888"
	.balign 4
	.asciz "_BGR__888"
	.balign 4
	.asciz "IDX_4_ABGR_1555"
	.asciz "IDX_4__BGR__555"
	.asciz "IDX_4_ABGR_8888"
	.asciz "IDX_4__BGR__888"
	.asciz "IDX_8_ABGR_1555"
	.asciz "IDX_8__BGR__555"
	.asciz "IDX_8_ABGR_8888"
	.asciz "IDX_8__BGR__888"
	.asciz "IDX_8_A_8"
	.balign 4
	.asciz "IDX_8_I_8"
	.balign 4
	.asciz "IDX_4_A_4"
	.balign 4
	.asciz "IDX_4_I_4"
	.balign 4
	.asciz "<UNKNOWN>"
	.balign 4
	.asciz "RAD:%6.2f VERTS:%-3d TRIS:%-3d OBJ:%-3d DEF:%-3d IDnum:%-4d"
	.asciz "POS:%7.2f %7.2f %7.2f  PYR:%7.2f %7.2f %7.2f"
	.balign 4
	.asciz "MenuSelect = %s\n"
	.balign 4
	.asciz "MenuActive = %s\n"
	.balign 4
	.asciz "MenuActive = TOP\n"
	.balign 4

.global lbl_801151C8
lbl_801151C8:

	# ROM: 0x1121C8
	.asciz "aaawhite"
	.balign 4
	.4byte 0

.global lbl_801151D8
lbl_801151D8:

	# ROM: 0x1121D8
	.asciz "---- ALLOC World Save State [%dK]\n"
	.balign 4
	.asciz "colworlds"
	.balign 4
	.asciz "AAAWHITE"
	.balign 4

.global lbl_80115214
lbl_80115214:

	# ROM: 0x112214
	.asciz "----- WORLD %s, MEM: %d -> "

.global lbl_80115230
lbl_80115230:

	# ROM: 0x112230
	.asciz "%d  [WORLD=%dK]\n"
	.balign 4

.global lbl_80115244
lbl_80115244:

	# ROM: 0x112244
	.asciz "---- ALLOC World Data [%dK]\n"
	.balign 4

.global lbl_80115264
lbl_80115264:

	# ROM: 0x112264
	.asciz "World psys: bad name: %s"
	.balign 4

.global lbl_80115280
lbl_80115280:

	# ROM: 0x112280
	.asciz "Unable to find world psys '%c'"
	.balign 4

.global lbl_801152A0
lbl_801152A0:

	# ROM: 0x1122A0
	.asciz "World obj with dynamic parent"
	.balign 4

.global lbl_801152C0
lbl_801152C0:

	# ROM: 0x1122C0
	.asciz "IDLE2_LOOP"
	.balign 4

.global lbl_801152CC
lbl_801152CC:

	# ROM: 0x1122CC
	.asciz "DEFENDLEFT"
	.balign 4

.global lbl_801152D8
lbl_801152D8:

	# ROM: 0x1122D8
	.asciz "DEFENDRIGHT"

.global lbl_801152E4
lbl_801152E4:

	# ROM: 0x1122E4
	.asciz "DEFENDBACK"
	.balign 4

.global lbl_801152F0
lbl_801152F0:

	# ROM: 0x1122F0
	.asciz "STRAFE_WLKF1"
	.balign 4

.global lbl_80115300
lbl_80115300:

	# ROM: 0x112300
	.asciz "STRAFE_WLKF2"
	.balign 4

.global lbl_80115310
lbl_80115310:

	# ROM: 0x112310
	.asciz "STRAFE_WLKB1"
	.balign 4

.global lbl_80115320
lbl_80115320:

	# ROM: 0x112320
	.asciz "STRAFE_WLKB2"
	.balign 4

.global lbl_80115330
lbl_80115330:

	# ROM: 0x112330
	.asciz "STRAFE_WLKL1"
	.balign 4

.global lbl_80115340
lbl_80115340:

	# ROM: 0x112340
	.asciz "STRAFE_WLKL2"
	.balign 4

.global lbl_80115350
lbl_80115350:

	# ROM: 0x112350
	.asciz "STRAFE_WLKR1"
	.balign 4

.global lbl_80115360
lbl_80115360:

	# ROM: 0x112360
	.asciz "STRAFE_WLKR2"
	.balign 4

.global lbl_80115370
lbl_80115370:

	# ROM: 0x112370
	.asciz "SHIELD_READY"
	.balign 4

.global lbl_80115380
lbl_80115380:

	# ROM: 0x112380
	.asciz "SHIELD_RUN"
	.balign 4

.global lbl_8011538C
lbl_8011538C:

	# ROM: 0x11238C
	.asciz "HITREACT"
	.balign 4

.global lbl_80115398
lbl_80115398:

	# ROM: 0x112398
	.asciz "DEATHGRABS"
	.balign 4

.global lbl_801153A4
lbl_801153A4:

	# ROM: 0x1123A4
	.asciz "DEATHGRAB"
	.balign 4

.global lbl_801153B0
lbl_801153B0:

	# ROM: 0x1123B0
	.asciz "DEATHGRABR"
	.balign 4

.global lbl_801153BC
lbl_801153BC:

	# ROM: 0x1123BC
	.asciz "ATTSTART"
	.balign 4

.global lbl_801153C8
lbl_801153C8:

	# ROM: 0x1123C8
	.asciz "ATTSLOW1"
	.balign 4

.global lbl_801153D4
lbl_801153D4:

	# ROM: 0x1123D4
	.asciz "ATTSLOW1R"
	.balign 4

.global lbl_801153E0
lbl_801153E0:

	# ROM: 0x1123E0
	.asciz "ATTPWRACLOSE"
	.balign 4

.global lbl_801153F0
lbl_801153F0:

	# ROM: 0x1123F0
	.asciz "ATTPWRACLOSER"
	.balign 4

.global lbl_80115400
lbl_80115400:

	# ROM: 0x112400
	.asciz "ATTPWRAMED"
	.balign 4

.global lbl_8011540C
lbl_8011540C:

	# ROM: 0x11240C
	.asciz "ATTPWRAMEDR"

.global lbl_80115418
lbl_80115418:

	# ROM: 0x112418
	.asciz "ATTQUICK1"
	.balign 4

.global lbl_80115424
lbl_80115424:

	# ROM: 0x112424
	.asciz "ATTQUICK2"
	.balign 4

.global lbl_80115430
lbl_80115430:

	# ROM: 0x112430
	.asciz "ATTQUICK3"
	.balign 4

.global lbl_8011543C
lbl_8011543C:

	# ROM: 0x11243C
	.asciz "ATTQUICK2R"
	.balign 4

.global lbl_80115448
lbl_80115448:

	# ROM: 0x112448
	.asciz "ATTQUICK3R"
	.balign 4

.global lbl_80115454
lbl_80115454:

	# ROM: 0x112454
	.asciz "ATTQ3RIGHT"
	.balign 4

.global lbl_80115460
lbl_80115460:

	# ROM: 0x112460
	.asciz "ATTQ2RIGHT"
	.balign 4

.global lbl_8011546C
lbl_8011546C:

	# ROM: 0x11246C
	.asciz "ATTQ3RIGHTR"

.global lbl_80115478
lbl_80115478:

	# ROM: 0x112478
	.asciz "ATTQ2RIGHTR"

.global lbl_80115484
lbl_80115484:

	# ROM: 0x112484
	.asciz "ATTQ3LEFT"
	.balign 4

.global lbl_80115490
lbl_80115490:

	# ROM: 0x112490
	.asciz "ATTQ2LEFT"
	.balign 4

.global lbl_8011549C
lbl_8011549C:

	# ROM: 0x11249C
	.asciz "ATTQ3LEFTR"
	.balign 4

.global lbl_801154A8
lbl_801154A8:

	# ROM: 0x1124A8
	.asciz "ATTQ2LEFTR"
	.balign 4

.global lbl_801154B4
lbl_801154B4:

	# ROM: 0x1124B4
	.asciz "ATTQ2180"
	.balign 4

.global lbl_801154C0
lbl_801154C0:

	# ROM: 0x1124C0
	.asciz "ATTQ3180"
	.balign 4

.global lbl_801154CC
lbl_801154CC:

	# ROM: 0x1124CC
	.asciz "ATTQ2180R"
	.balign 4

.global lbl_801154D8
lbl_801154D8:

	# ROM: 0x1124D8
	.asciz "ATTQ3180R"
	.balign 4

.global lbl_801154E4
lbl_801154E4:

	# ROM: 0x1124E4
	.asciz "ATTQ2180L"
	.balign 4

.global lbl_801154F0
lbl_801154F0:

	# ROM: 0x1124F0
	.asciz "ATTQ3180L"
	.balign 4

.global lbl_801154FC
lbl_801154FC:

	# ROM: 0x1124FC
	.asciz "ATTQ2180LR"
	.balign 4

.global lbl_80115508
lbl_80115508:

	# ROM: 0x112508
	.asciz "ATTQ3180LR"
	.balign 4

.global lbl_80115514
lbl_80115514:

	# ROM: 0x112514
	.asciz "ATTSTEP1"
	.balign 4

.global lbl_80115520
lbl_80115520:

	# ROM: 0x112520
	.asciz "ATTSTEP2"
	.balign 4

.global lbl_8011552C
lbl_8011552C:

	# ROM: 0x11252C
	.asciz "ATTSTEP3"
	.balign 4

.global lbl_80115538
lbl_80115538:

	# ROM: 0x112538
	.asciz "ATTSTEP2R"
	.balign 4

.global lbl_80115544
lbl_80115544:

	# ROM: 0x112544
	.asciz "ATTSTEP3R"
	.balign 4

.global lbl_80115550
lbl_80115550:

	# ROM: 0x112550
	.asciz "ATTQ3TOSTEP1"
	.balign 4

.global lbl_80115560
lbl_80115560:

	# ROM: 0x112560
	.asciz "ATTQ3TOSTEP1R"
	.balign 4

.global lbl_80115570
lbl_80115570:

	# ROM: 0x112570
	.asciz "ATTWALK2"
	.balign 4

.global lbl_8011557C
lbl_8011557C:

	# ROM: 0x11257C
	.asciz "ATTWALK2R"
	.balign 4

.global lbl_80115588
lbl_80115588:

	# ROM: 0x112588
	.asciz "STRAFE_ATKF1"
	.balign 4

.global lbl_80115598
lbl_80115598:

	# ROM: 0x112598
	.asciz "STRAFE_ATKF2"
	.balign 4

.global lbl_801155A8
lbl_801155A8:

	# ROM: 0x1125A8
	.asciz "STRAFE_ATKB1"
	.balign 4

.global lbl_801155B8
lbl_801155B8:

	# ROM: 0x1125B8
	.asciz "STRAFE_ATKB2"
	.balign 4

.global lbl_801155C8
lbl_801155C8:

	# ROM: 0x1125C8
	.asciz "STRAFE_ATKL1"
	.balign 4

.global lbl_801155D8
lbl_801155D8:

	# ROM: 0x1125D8
	.asciz "STRAFE_ATKL2"
	.balign 4

.global lbl_801155E8
lbl_801155E8:

	# ROM: 0x1125E8
	.asciz "STRAFE_ATKR1"
	.balign 4

.global lbl_801155F8
lbl_801155F8:

	# ROM: 0x1125F8
	.asciz "STRAFE_ATKR2"
	.balign 4

.global lbl_80115608
lbl_80115608:

	# ROM: 0x112608
	.asciz "ATTLOWKR"
	.balign 4

.global lbl_80115614
lbl_80115614:

	# ROM: 0x112614
	.asciz "ATTPWRALOW"
	.balign 4

.global lbl_80115620
lbl_80115620:

	# ROM: 0x112620
	.asciz "ATTPWRALOWR"

.global lbl_8011562C
lbl_8011562C:

	# ROM: 0x11262C
	.asciz "COMBOACT1"
	.balign 4

.global lbl_80115638
lbl_80115638:

	# ROM: 0x112638
	.asciz "COMBOACT2"
	.balign 4

.global lbl_80115644
lbl_80115644:

	# ROM: 0x112644
	.asciz "COMBOACT3"
	.balign 4

.global lbl_80115650
lbl_80115650:

	# ROM: 0x112650
	.asciz "ATTPWRATHROW"
	.balign 4

.global lbl_80115660
lbl_80115660:

	# ROM: 0x112660
	.asciz "ATTPWRATHROWR"
	.balign 4

.global lbl_80115670
lbl_80115670:

	# ROM: 0x112670
	.asciz "ATTFIREL"
	.balign 4

.global lbl_8011567C
lbl_8011567C:

	# ROM: 0x11267C
	.asciz "ATTFIRELR"
	.balign 4

.global lbl_80115688
lbl_80115688:

	# ROM: 0x112688
	.asciz "ATTFIRER"
	.balign 4

.global lbl_80115694
lbl_80115694:

	# ROM: 0x112694
	.asciz "ATTFIRERR"
	.balign 4

.global lbl_801156A0
lbl_801156A0:

	# ROM: 0x1126A0
	.asciz "ATTBREATHE"
	.balign 4

.global lbl_801156AC
lbl_801156AC:

	# ROM: 0x1126AC
	.asciz "ATTBREATHER"

.global lbl_801156B8
lbl_801156B8:

	# ROM: 0x1126B8
	.asciz "ATTCHOPR"
	.balign 4

.global lbl_801156C4
lbl_801156C4:

	# ROM: 0x1126C4
	.asciz "THROWPOTIONS"
	.balign 4

.global lbl_801156D4
lbl_801156D4:

	# ROM: 0x1126D4
	.asciz "THROWPOTIONR"
	.balign 4

.global lbl_801156E4
lbl_801156E4:

	# ROM: 0x1126E4
	.asciz "WEBREACT"
	.balign 4

.global lbl_801156F0
lbl_801156F0:

	# ROM: 0x1126F0
	.asciz "FALLDOWN"
	.balign 4

.global lbl_801156FC
lbl_801156FC:

	# ROM: 0x1126FC
	.asciz "FALLFRNT"
	.balign 4

.global lbl_80115708
lbl_80115708:

	# ROM: 0x112708
	.asciz "COMBOWAR1"
	.balign 4

.global lbl_80115714
lbl_80115714:

	# ROM: 0x112714
	.asciz "COMBOWAR2"
	.balign 4

.global lbl_80115720
lbl_80115720:

	# ROM: 0x112720
	.asciz "COMBOWAR3"
	.balign 4

.global lbl_8011572C
lbl_8011572C:

	# ROM: 0x11272C
	.asciz "COMBOVAL"
	.balign 4

.global lbl_80115738
lbl_80115738:

	# ROM: 0x112738
	.asciz "COMBOWIZ"
	.balign 4

.global lbl_80115744
lbl_80115744:

	# ROM: 0x112744
	.asciz "COMBOARC"
	.balign 4

.global lbl_80115750
lbl_80115750:

	# ROM: 0x112750
	.asciz "COMBODWF1"
	.balign 4

.global lbl_8011575C
lbl_8011575C:

	# ROM: 0x11275C
	.asciz "COMBODWF2"
	.balign 4

.global lbl_80115768
lbl_80115768:

	# ROM: 0x112768
	.asciz "COMBODWF3"
	.balign 4

.global lbl_80115774
lbl_80115774:

	# ROM: 0x112774
	.asciz "COMBOKNI"
	.balign 4

.global lbl_80115780
lbl_80115780:

	# ROM: 0x112780
	.asciz "COMBOSOR"
	.balign 4

.global lbl_8011578C
lbl_8011578C:

	# ROM: 0x11278C
	.asciz "COMBOJES"
	.balign 4

.global lbl_80115798
lbl_80115798:

	# ROM: 0x112798
	.asciz "WALKTOREADY"

.global lbl_801157A4
lbl_801157A4:

	# ROM: 0x1127A4
	.asciz "READYTOWALK"

.global lbl_801157B0
lbl_801157B0:

	# ROM: 0x1127B0
	.asciz "ATTACK1R"
	.balign 4

.global lbl_801157BC
lbl_801157BC:

	# ROM: 0x1127BC
	.asciz "ATTACK2R"
	.balign 4

.global lbl_801157C8
lbl_801157C8:

	# ROM: 0x1127C8
	.asciz "ATTACK3R"
	.balign 4

.global lbl_801157D4
lbl_801157D4:

	# ROM: 0x1127D4
	.asciz "ATTACK4R"
	.balign 4

.global lbl_801157E0
lbl_801157E0:

	# ROM: 0x1127E0
	.asciz "ATTACK5R"
	.balign 4

.global lbl_801157EC
lbl_801157EC:

	# ROM: 0x1127EC
	.asciz "RUNATTACK1"
	.balign 4

.global lbl_801157F8
lbl_801157F8:

	# ROM: 0x1127F8
	.asciz "RUNATTACK2"
	.balign 4

.global lbl_80115804
lbl_80115804:

	# ROM: 0x112804
	.asciz "ATTTOREADY"
	.balign 4

.global lbl_80115810
lbl_80115810:

	# ROM: 0x112810
	.asciz "ACTION:%s NEXT:%s D:%s INT:%d RPT:%d DIDT:%d"
	.balign 4

.global lbl_80115840
lbl_80115840:

	# ROM: 0x112840
	.asciz "  SEQ:%s  frame:%.1f/%d      "
	.balign 4

.global lbl_80115860
lbl_80115860:

	# ROM: 0x112860
	.asciz "Stop Watch #0"
	.balign 4
	.asciz "Stop Watch #1"
	.balign 4
	.asciz "Stop Watch #2"
	.balign 4
	.asciz "Stop Watch #3"
	.balign 4
	.asciz "Stop Watch #4"
	.balign 4
	.asciz "Stop Watch #5"
	.balign 4
	.asciz "Stop Watch #6"
	.balign 4
	.asciz "Stop Watch #7"
	.balign 4
	.asciz "Stop Watch #8"
	.balign 4
	.asciz "Stop Watch #9"
	.balign 4
	.asciz "Stop Watch #10"
	.balign 4
	.asciz "An error has occurred.\n\nTurn the power off and refer\nto the Nintendo GameCube TM\nInstruction Booklet\nfor further instructions."
	.balign 4
	.asciz "Please close the Disc Cover."
	.balign 4
	.asciz "Please insert the Gauntlet Dark Legacy Game Disc."
	.balign 4
	.asciz "The Game Disc could not be read.\n\nPlease read the\nNintendo GameCube TM Instruction\nBooklet for more information."
	.balign 4
	.asciz "Unsupported function."
	.balign 4

.global lbl_80115A70
lbl_80115A70:

	# ROM: 0x112A70
	.asciz "invalid block type"
	.balign 4
	.asciz "invalid stored block lengths"
	.balign 4
	.asciz "too many length or distance symbols"
	.asciz "invalid bit length repeat"
	.balign 4
	.4byte 0

.global lbl_80115AE8
lbl_80115AE8:

	# ROM: 0x112AE8
	.asciz "invalid literal/length code"

.global lbl_80115B04
lbl_80115B04:

	# ROM: 0x112B04
	.asciz "invalid distance code"
	.balign 4
	.4byte 0

.global lbl_80115B20
lbl_80115B20:

	# ROM: 0x112B20
	.asciz "invalid distance code"
	.balign 4

.global lbl_80115B38
lbl_80115B38:

	# ROM: 0x112B38
	.asciz "invalid literal/length code"
	.4byte 0

.global lbl_80115B58
lbl_80115B58:

	# ROM: 0x112B58
	.asciz "unknown compression method"
	.balign 4
	.asciz "invalid window size"
	.asciz "incorrect header check"
	.balign 4
	.asciz "need dictionary"
	.asciz "incorrect data check"
	.balign 4

.global lbl_80115BC8
lbl_80115BC8:

	# ROM: 0x112BC8
	.asciz "oversubscribed literal/length tree"
	.balign 4
	.asciz "incomplete literal/length tree"
	.balign 4

.global lbl_80115C0C
lbl_80115C0C:

	# ROM: 0x112C0C
	.asciz "oversubscribed dynamic bit lengths tree"

.global lbl_80115C34
lbl_80115C34:

	# ROM: 0x112C34
	.asciz "incomplete dynamic bit lengths tree"

.global lbl_80115C58
lbl_80115C58:

	# ROM: 0x112C58
	.asciz "Too Many Temp Blits"

.global lbl_80115C6C
lbl_80115C6C:

	# ROM: 0x112C6C
	.asciz "Too Many Quads"
	.balign 4

.global lbl_80115C7C
lbl_80115C7C:

	# ROM: 0x112C7C
	.asciz "Attempt to order blits in different nodes"
	.balign 4

.global lbl_80115CA8
lbl_80115CA8:

	# ROM: 0x112CA8
	.asciz "Attempt to add too many blits"
	.balign 4

.global lbl_80115CC8
lbl_80115CC8:

	# ROM: 0x112CC8
	.asciz "DRAWBLIT COULD NOT LOAD TEX"
	.4byte 0

.global lbl_80115CE8
lbl_80115CE8:

	# ROM: 0x112CE8
	.asciz "MBLockMessages > max"
	.balign 4

.global lbl_80115D00
lbl_80115D00:

	# ROM: 0x112D00
	.asciz "TOO MANY DRAWTEXT MESSAGES: %d"
	.balign 4

.global lbl_80115D20
lbl_80115D20:

	# ROM: 0x112D20
	.asciz "TOO MANY DRAWTEXT CHARACTERS: %d"
	.balign 4

.global lbl_80115D44
lbl_80115D44:

	# ROM: 0x112D44
	.asciz "MBNewFont: MBNewBlit failed"

.global lbl_80115D60
lbl_80115D60:

	# ROM: 0x112D60
	.asciz "Too many fonts"
	.balign 4

.global lbl_80115D70
lbl_80115D70:

	# ROM: 0x112D70
	.4byte 0xBE99999A
	.4byte 0xBFB33333
	.4byte 0x3F800000
	.asciz "Too many lights"
	.4byte 0

.global lbl_80115D90
lbl_80115D90:

	# ROM: 0x112D90
	.asciz "MB_MAIN.CPP:__LINE__"
	.balign 4

.global lbl_80115DA8
lbl_80115DA8:

	# ROM: 0x112DA8
	.asciz "objects.ngc"

.global lbl_80115DB4
lbl_80115DB4:

	# ROM: 0x112DB4
	.asciz "textures.ngc"
	.balign 4
	.asciz "MBOX loading(%d): %s/objects.ps2 [Time=%d]\n"
	.asciz "MBOX loading(%d): %s/textures.ps2 [Time=%d]\n"
	.balign 4
	.asciz "MBOX loading(%d) %s: done [TIME=%d]\n"
	.balign 4
	.asciz "MBOX_BGLoadModelStart: bg_model >= 0"
	.balign 4
	.asciz "============ BGLOAD model[%d]=%s alloc=%x\n"
	.balign 4
	.asciz "=== LFIXED model[%d]=%-15s, size=0x%08x\n"
	.balign 4
	.asciz "model[%d]=%s load size (%x) > alloc size (%x)\n"
	.balign 4
	.asciz "tex %s load size (%x) > alloc size (%x)\n"
	.balign 4

.global lbl_80115F24
lbl_80115F24:

	# ROM: 0x112F24
	.asciz "model[%d]=%s has a bad version (%08x) (want %08x)\n"
	.balign 4
	.asciz "MBOX_AllocModel: > max models"
	.balign 4
	.asciz "Not enough mem to alloc model"
	.balign 4
	.asciz "<NO DESC>"
	.balign 4
	.asciz "==== ALLOC model[%d]=%-16s, MEM:%06dk -> "
	.balign 4
	.asciz "%06dK  [GEO=%dK TEX=%dK]\n"
	.balign 4
	.asciz "AAANULLOBJ"
	.balign 4
	.asciz "MBOX_FindObject: %s(%s)"

.global lbl_80116010
lbl_80116010:

	# ROM: 0x113010
	.asciz "Bad MBSetObject"

.global lbl_80116020
lbl_80116020:

	# ROM: 0x113020
	.asciz "mb_objects.c:__LINE__"
	.balign 4

.global lbl_80116038
lbl_80116038:

	# ROM: 0x113038
	.asciz "TOO MANY PSYS OBJECTS: %d"
	.balign 4

.global lbl_80116054
lbl_80116054:

	# ROM: 0x113054
	.asciz "TOO MANY ALPHA DIST OBJECTS: %d"

.global lbl_80116074
lbl_80116074:

	# ROM: 0x113074
	.asciz "TOO MANY ALPHA SORT OBJECTS: %d"
	.4byte 0

.global lbl_80116098
lbl_80116098:

	# ROM: 0x113098
	.asciz "MBRomTexPtr: texidx < 0"

.global lbl_801160B0
lbl_801160B0:

	# ROM: 0x1130B0
	.asciz "Too many UV Scale Add nodes active\n"

.global lbl_801160D4
lbl_801160D4:

	# ROM: 0x1130D4
	.asciz "Too many nodes"
	.balign 4
	.4byte 0

.global lbl_801160E8
lbl_801160E8:

	# ROM: 0x1130E8
	.asciz "MATH ERROR"
	.balign 4
	.asciz " VERSION %s"
	.asciz "\n========== FATAL ERROR ===========\n"
	.balign 4
	.asciz "\n==================================\n"
	.balign 4

.global lbl_80116150
lbl_80116150:

	# ROM: 0x113150
	.asciz "\n--> ERROR: "
	.balign 4

.global lbl_80116160
lbl_80116160:

	# ROM: 0x113160
	.asciz "\n========== ERRORS ========== \n"
	.asciz "\n>>> BREAKPOINT! <<<\n\n"
	.balign 4

.global lbl_80116198
lbl_80116198:

	# ROM: 0x113198
	.asciz "sin bad input: %f"
	.balign 4
	.4byte 0

.global lbl_801161B0
lbl_801161B0:

	# ROM: 0x1131B0
	.asciz "Temporary high memory in use by somebody else when StartFileRead called.  Check if another file is being read!"
	.balign 4
	.asciz "Too many open files: %d"

.global lbl_80116238
lbl_80116238:

	# ROM: 0x113238
	.asciz "/gauntlet/"
	.balign 4
	.asciz "Can't open: %s.\n"
	.balign 4

.global lbl_80116258
lbl_80116258:

	# ROM: 0x113258
	.asciz "Error decompressing file. Can not contine."
	.balign 4

.global lbl_80116284
lbl_80116284:

	# ROM: 0x113284
	.asciz "Timeout serving file %s (1)"

.global lbl_801162A0
lbl_801162A0:

	# ROM: 0x1132A0
	.asciz "Available Memory = %d\n"
	.balign 4

.global lbl_801162B8
lbl_801162B8:

	# ROM: 0x1132B8
	.asciz "Too Many Mem locks"
	.balign 4

.global lbl_801162CC
lbl_801162CC:

	# ROM: 0x1132CC
	.asciz "GetMemBase() called while mem reserved"
	.balign 4

.global lbl_801162F4
lbl_801162F4:

	# ROM: 0x1132F4
	.asciz "AllocMem() called while mem reserved"
	.balign 4

.global lbl_8011631C
lbl_8011631C:

	# ROM: 0x11331C
	.asciz "AllocMem failed: %d bytes, exceeds free by %d bytes"

.global lbl_80116350
lbl_80116350:

	# ROM: 0x113350
	.asciz "AllocHiMem failed: %d bytes, exceeds free by %d bytes"
	.balign 4

.global lbl_80116388
lbl_80116388:

	# ROM: 0x113388
	.asciz "File read overflowed: %s size:%d max:%d"
	.asciz "AllocFile: Read failed."
	.asciz "==== ALLOC FILE=%s/%s, MEM:%06dk -> %06dk [%dk]\n"
	.balign 4
	.asciz "Can't load: %s, failed on open.\n"
	.balign 4
	.asciz "Can't load: %s, failed on Lseek for size.\n"
	.balign 4
	.4byte 0

.global lbl_80116450
lbl_80116450:

	# ROM: 0x113450
	.4byte 0x00FF0000
	.4byte 0x00FF00FF
	.4byte 0x0000FF00
	.4byte 0x00FF00FF
	.4byte 0x0000FF00
	.4byte 0x00FF00FF
	.4byte 0x0000FF00
	.4byte 0x00FF00FF
	.asciz "   60          30          20 "
	.balign 4
	.asciz "(%03d)%d"
	.balign 4
	.asciz "   60    30    20    15    12 "
	.balign 4
	.4byte 0

.global lbl_801164C0
lbl_801164C0:

	# ROM: 0x1134C0
	.asciz "PB_ERROR.C:__LINE__"
	.4byte 0

.global lbl_801164D8
lbl_801164D8:

	# ROM: 0x1134D8
	.asciz "PBFB_NOCHANGE"
	.balign 4

.global lbl_801164E8
lbl_801164E8:

	# ROM: 0x1134E8
	.asciz "PBFB_CLOSED"

.global lbl_801164F4
lbl_801164F4:

	# ROM: 0x1134F4
	.asciz "PBFB_FIRST"
	.balign 4

.global lbl_80116500
lbl_80116500:

	# ROM: 0x113500
	.asciz "PBFB_640_224_C16_Z24"
	.balign 4

.global lbl_80116518
lbl_80116518:

	# ROM: 0x113518
	.asciz "PBFB_640_448_C16_Z24"
	.balign 4

.global lbl_80116530
lbl_80116530:

	# ROM: 0x113530
	.asciz "PBFB_640_224_C16_Z16"
	.balign 4

.global lbl_80116548
lbl_80116548:

	# ROM: 0x113548
	.asciz "PBFB_640_448_C16_Z16"
	.balign 4

.global lbl_80116560
lbl_80116560:

	# ROM: 0x113560
	.asciz "PBFB_LAST"
	.balign 4

.global lbl_8011656C
lbl_8011656C:

	# ROM: 0x11356C
	.asciz "PB_FRAME.C:__LINE__"

.global lbl_80116580
lbl_80116580:

	# ROM: 0x113580
	.asciz "ERROR: pbInitGlobal did not init all modules: %08x\n"
	.4byte 0

.global lbl_801165B8
lbl_801165B8:

	# ROM: 0x1135B8
	.asciz "PB_ODB_NOT_DRAWING_OBJECT"
	.balign 4

.global lbl_801165D4
lbl_801165D4:

	# ROM: 0x1135D4
	.asciz "PB_ODB_DONE"

.global lbl_801165E0
lbl_801165E0:

	# ROM: 0x1135E0
	.asciz "PB_ODB_OBJ_GETTEX"
	.balign 4

.global lbl_801165F4
lbl_801165F4:

	# ROM: 0x1135F4
	.asciz "PB_ODB_OBJ_GETTEX_DONE"
	.balign 4

.global lbl_8011660C
lbl_8011660C:

	# ROM: 0x11360C
	.asciz "PB_ODB_OBJ_DRAWING"
	.balign 4

.global lbl_80116620
lbl_80116620:

	# ROM: 0x113620
	.asciz "PB_ODB_OBJ_DRAWING_DONE"

.global lbl_80116638
lbl_80116638:

	# ROM: 0x113638
	.asciz "PB_ODB_END"
	.balign 4
	.asciz "BINARY SEARCH: Is object visible? (yy/nn)\n"
	.balign 4
	.asciz "Binary search selected model %d\n"
	.balign 4
	.asciz "Could not locate model with binary search\n"
	.balign 4
	.asciz "Binary search selected obj %d\n"
	.balign 4
	.asciz "Could not locate object with binary search\n"
	.asciz "SELECTED:  Model"
	.balign 4
	.asciz "s %d - %d;"
	.balign 4
	.asciz "   Object"
	.balign 4
	.asciz "s %d - %d"
	.balign 4
	.asciz "  (all objs);"
	.balign 4
	.asciz "   Subobj %d"
	.balign 4
	.asciz "   (all %d subobjs);"
	.balign 4
	.asciz "   Triangle %d"
	.balign 4
	.asciz "   (%d tris in obj);"
	.balign 4

.global lbl_801167A4
lbl_801167A4:

	# ROM: 0x1137A4
	.asciz "NO-ObjDef"
	.balign 4

.global lbl_801167B0
lbl_801167B0:

	# ROM: 0x1137B0
	.asciz "Obj Textures Larger than a page"
	.asciz "pb_objects.c:__LINE__"
	.balign 4
	.asciz "OBJECT SINGLE STEP TIMEOUT\n"
	.asciz "STATE:    %s\n"
	.balign 4
	.asciz "  node=   0x%08x\n"
	.balign 4
	.asciz "  objidx= 0x%08x\n"
	.balign 4
	.asciz "OBJECT = '%s'\n"
	.balign 4
	.asciz "Status from SyncPath:\n"
	.balign 4
	.asciz "    GIF:  %s\n"
	.balign 4
	.asciz "    VU1:  %s\n"
	.balign 4
	.asciz "    VIF1: %s\n"
	.balign 4
	.asciz "    DMA2: %s\n"
	.balign 4
	.asciz "    DMA1: %s\n"
	.balign 4
	.asciz "===========================\n"
	.balign 4
	.4byte 0

.global lbl_801168D8
lbl_801168D8:

	# ROM: 0x1138D8
	.asciz "w look=<%6.3Lf %6.3Lf %6.3Lf>  "
	.asciz "w up=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "w right=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "c up=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "c right=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "m up=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "m right=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "nm up=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "nm right=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "s up=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "s right=<%6.3Lf %6.3Lf %6.3Lf>  "
	.balign 4
	.asciz "yaw,pitch= %4d %4d"
	.balign 4

.global lbl_80116A60
lbl_80116A60:

	# ROM: 0x113A60
	.asciz "txsh: %6.2Lf %6.2Lf  %6.3Lf %6.3Lf  "
	.balign 4

.global lbl_80116A88
lbl_80116A88:

	# ROM: 0x113A88
	.asciz "pbSetDODrawRegs: Texture not loaded"
	.4byte 0

.global lbl_80116AB0
lbl_80116AB0:

	# ROM: 0x113AB0
	.4byte 0
	.4byte 0x0000FFFF
	.4byte 0x00007FFF
	.4byte 0xFFFF0000

.global lbl_80116AC0
lbl_80116AC0:

	# ROM: 0x113AC0
	.asciz "Lightmaps > %dK, %d/%d: %s"
	.balign 4
	.4byte 0

.global lbl_80116AE0
lbl_80116AE0:

	# ROM: 0x113AE0
	.asciz "PushMatrix: Too many levels"

.global lbl_80116AFC
lbl_80116AFC:

	# ROM: 0x113AFC
	.asciz "ALPHATREE_NODE child of ALPHATREE_NODE"
	.balign 4
	.4byte 0

.global lbl_80116B28
lbl_80116B28:

	# ROM: 0x113B28
	.asciz "=== cam_pitch/yaw = %4.2Lf  %4.2Lf\n"

.global lbl_80116B4C
lbl_80116B4C:

	# ROM: 0x113B4C
	.asciz "MBSetCurrentWindow: Bad window id.\n"

.global lbl_80116B70
lbl_80116B70:

	# ROM: 0x113B70
	.asciz "                00000000000000000123456789abcdef"
	.balign 4
	.asciz "0123456789ABCDEF"
	.balign 4
	.asciz "bug in vsprintf: bad base"
	.balign 4
	.4byte 0

.global lbl_80116BD8
lbl_80116BD8:

	# ROM: 0x113BD8
	.asciz "\nNow, try to find memory info file...\n\n"
	.asciz "/meminfo.bin"
	.balign 4
	.asciz "\nCan't find memory info file. Use /XXX toolname/ to maximize available\n"
	.asciz "memory space. For now, we only use the first %dMB.\n"

.global lbl_80116C8C
lbl_80116C8C:

	# ROM: 0x113C8C
	.asciz "DEMOInit.c"
	.balign 4
	.asciz "An error occurred when issuing read to /meminfo.bin\n"
	.balign 4
	.asciz "start: 0x%08x, end: 0x%08x\n"
	.asciz "Removed 0x%08x - 0x%08x from the current heap\n"
	.balign 4

.global lbl_80116D1C
lbl_80116D1C:

	# ROM: 0x113D1C
	.asciz "DEMOInit: invalid TV format\n"
	.balign 4
	.asciz "---------WARNING : ABORTING FRAME----------\n"
	.balign 4
	.4byte 0

.global lbl_80116D70
lbl_80116D70:

	# ROM: 0x113D70
	.asciz "FIREWORK"
	.balign 4

.global lbl_80116D7C
lbl_80116D7C:

	# ROM: 0x113D7C
	.asciz "FIREWORKG"
	.balign 4
	.asciz "<--- mid_idx"
	.balign 4
	.asciz "<--- fad_idx"
	.balign 4
	.asciz "<--- end_idx"
	.balign 4
	.4byte 0
	.4byte 0
	.4byte 0x41800000
	.4byte 0
	.4byte 0x41800000
	.4byte 0x41800000
	.4byte 0
	.4byte 0x41800000
	.4byte 0xBF800000
	.4byte 0xBF800000
	.4byte 0x3F800000
	.4byte 0xBF800000
	.4byte 0x3F800000
	.4byte 0x3F800000
	.4byte 0xBF800000
	.4byte 0x3F800000

.global lbl_80116DF8
lbl_80116DF8:

	# ROM: 0x113DF8
	.asciz "MBTraversePsys: PSYS node with psys=0"
	.balign 4
	.asciz "No mem for world psys %s"
	.balign 4
	.asciz "No mem for psys id=%d"
	.balign 4
	.asciz "PSYS requires too many bits (%d+%d+1=%d > %d)\n"
	.balign 4
	.asciz "setWorldParms: WORLDPSYS type is bad"
	.balign 4

.global lbl_80116EAC
lbl_80116EAC:

	# ROM: 0x113EAC
	.asciz "Setting PSYS attribute after draw begins"
	.balign 4

.global lbl_80116ED8
lbl_80116ED8:

	# ROM: 0x113ED8
	.asciz "particle2_a"

.global lbl_80116EE4
lbl_80116EE4:

	# ROM: 0x113EE4
	.asciz "particle2_xp"
	.balign 4

.global lbl_80116EF4
lbl_80116EF4:

	# ROM: 0x113EF4
	.asciz "MBPsysSetPParm: parm %d too big"

.global lbl_80116F14
lbl_80116F14:

	# ROM: 0x113F14
	.asciz "MBRemovePsys: Non psys node"

.global lbl_80116F30
lbl_80116F30:

	# ROM: 0x113F30
	.asciz "freePsysMem: bad free block. Error=%d"
	.balign 4

.global lbl_80116F58
lbl_80116F58:

	# ROM: 0x113F58
	.asciz "DCSERROR: "
	.balign 4

.global lbl_80116F64
lbl_80116F64:

	# ROM: 0x113F64
	.asciz "Duck nonzero (%d) -- resetting\n"
	.asciz "DCSFATAL: "
	.balign 4
	.asciz "OVERWRITE existing SAMPLE (%d)\n"
	.asciz "%d samples read OK -- %d PUNTED\n"
	.balign 4
	.asciz "BANK End of file in CALL list\n"
	.balign 4
	.asciz "BANK max call index exceeded\n"
	.balign 4
	.asciz "BANK max call instructions exceeded\n"
	.balign 4
	.asciz "BANK nCall exceeds size\n"
	.balign 4
	.asciz "BANK call list size mismatches header\n"
	.balign 4

.global lbl_80117080
lbl_80117080:

	# ROM: 0x114080
	.asciz "BANK max sample index exceeded\n"
	.asciz "No BANK header found\n"
	.balign 4
	.asciz "BankReadHeader NOT A BANK!\n"
	.asciz "BankReadHeader TOO MANY CALLS: 0x%04x > 0x4FF"
	.balign 4
	.asciz "Incomplete BANK header found\n"
	.balign 4
	.asciz "BankReadHeader BAD SIZE for call list"
	.balign 4
	.asciz "Unknown BANK version"
	.balign 4
	.asciz "Bank type %d.%03d,  made with VAGBANK v%d.%03d\n"

.global lbl_80117194
lbl_80117194:

	# ROM: 0x114194
	.asciz "VagParseHeader NOT A VAG!\n"
	.balign 4

.global lbl_801171B0
lbl_801171B0:

	# ROM: 0x1141B0
	.asciz "DCSERROR: "
	.balign 4
	.asciz "Invalid FileBuf\n"
	.balign 4
	.asciz "FileBufStart was never called!\n"
	.asciz "FileBuf read %d wanted %d (nfile %d)\n"
	.balign 4
	.asciz "FileBuf underrun by %d bytes\n"
	.balign 4

.global lbl_80117238
lbl_80117238:

	# ROM: 0x114238
	.asciz "OPEN FILE %s\n"
	.balign 4
	.asciz "No more FILEBUF handles\n"
	.balign 4
	.4byte 0

.global lbl_80117268
lbl_80117268:

	# ROM: 0x114268
	.asciz "dcs_driver"
	.balign 4
	.asciz "DCSERROR: "
	.balign 4
	.asciz "Ads HARD shutdown\n"
	.balign 4

.global lbl_80117294
lbl_80117294:

	# ROM: 0x114294
	.asciz "/gauntlet/"
	.balign 4

.global lbl_801172A0
lbl_801172A0:

	# ROM: 0x1142A0
	.asciz "MEMLOCK is saving your skin!\n"
	.balign 4
	.asciz "DCSERROR: "
	.balign 4
	.asciz "No more free MemBlk's\n"
	.balign 4
	.asciz "pool_free ran out of FREE blocks! increase pool_init() value\n"
	.balign 4
	.asciz "pool_dispose_and_alloc IGNORING already-allocated block 0x%08x\n"
	.asciz "pool_alloc IGNORING already-allocated block 0x%08x\n"
	.asciz "pool_alloc IGNORING 0-byte request\n"
	.asciz "pool_alloc failed %d bytes (%d biggest, %d total)\n"
	.balign 4
	.asciz "WARNING: pool_new rounding block up to next power of 2: 0x%08x\n"
	.asciz "DISPOSE code %d unknown\n"
	.balign 4
	.asciz "DCSFATAL: "
	.balign 4
	.asciz "LIST Null link\n"
	.asciz "LIST Bad node 0x%08x <- 0x%08x\n"
	.asciz "LIST Bad node 0x%08x -> 0x%08x\n"

.global lbl_801174A8
lbl_801174A8:

	# ROM: 0x1144A8
	.asciz "DCSERROR: "
	.balign 4

.global lbl_801174B4
lbl_801174B4:

	# ROM: 0x1144B4
	.asciz "SPU UNDERRUN, loop-glitch likely\n"
	.balign 4
	.asciz "AdsPutBuffer EOF -- %d bytes UNSENT\n"
	.balign 4
	.asciz "AdsPutBuffer overrun! %d bytes UNSENT\n"
	.balign 4

.global lbl_80117528
lbl_80117528:

	# ROM: 0x114528
	.asciz "Gauntlet/VQMovies/@"
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.asciz "Gauntlet/VQMovies/midway.avi"
	.balign 4

.global lbl_80117648
lbl_80117648:

	# ROM: 0x114648
	.asciz "MoviePlayer"

.global lbl_80117654
lbl_80117654:

	# ROM: 0x114654
	.asciz "MoviePlayerBase"
	.asciz "Gauntlet/VQMovies/KnotVQ.avi"
	.balign 4
	.asciz "MoviePlayer.cpp"
	.4byte 0

.global lbl_80117698
lbl_80117698:

	# ROM: 0x114698
	.asciz "Cardutilmainloop....\n"
	.balign 4

.global lbl_801176B0
lbl_801176B0:

	# ROM: 0x1146B0
	.asciz "Card is corrupted, format it?"
	.balign 4
	.asciz "The card is corrupted and unusable."
	.asciz "RESET RELEASED.."
	.balign 4

.global lbl_80117708
lbl_80117708:

	# ROM: 0x114708
	.asciz "RESET INVOKED.."
	.asciz "%s Press RESET to continue."
	.asciz "Assert Failed. File:%s Line:%d Press RESET to continue..."
	.balign 4

.global lbl_80117770
lbl_80117770:

	# ROM: 0x114770
	.asciz "Assert Failed: File:%s Line:%d"
	.balign 4

.global lbl_80117790
lbl_80117790:

	# ROM: 0x114790
	.asciz "Message: %s Press RESET to continue."
	.balign 4

.global lbl_801177B8
lbl_801177B8:

	# ROM: 0x1147B8
	.4byte 0
	.4byte 0
	.4byte 0x3F800000
	.4byte 0
	.4byte 0x3F000000
	.4byte 0x3F800000
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0x3F800000
	.4byte 0
	.4byte 0x3F800000
	.4byte 0x3F800000
	.4byte 0
	.4byte 0x3F800000

.global lbl_801177F8
lbl_801177F8:

	# ROM: 0x1147F8
	.asciz "PolyHeaderList Full"

.global lbl_8011780C
lbl_8011780C:

	# ROM: 0x11480C
	.asciz "PolyInstanceList Full"
	.balign 4

.global lbl_80117824
lbl_80117824:

	# ROM: 0x114824
	.asciz "PolyInstanceList Vertlist Full"
	.balign 4
	.4byte 0

.global lbl_80117848
lbl_80117848:

	# ROM: 0x114848
	.asciz "MetroTRK for Dolphin v0.5"
	.balign 4
	.4byte 0

.global lbl_80117868
lbl_80117868:

	# ROM: 0x114868
	.4byte 0
	.4byte 0xFFFFFFFF
	.4byte 0x00000001
	.4byte 0x00000001

.global lbl_80117878
lbl_80117878:

	# ROM: 0x114878
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000

.global lbl_8011788C
lbl_8011788C:

	# ROM: 0x11488C
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000

.global lbl_801178A0
lbl_801178A0:

	# ROM: 0x1148A0
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0x60000000
	.4byte 0
	.asciz "!std::exception!!std::bad_alloc!!"
	.balign 4
	.asciz "std::bad_alloc"
	.balign 4

.global lbl_801178EC
lbl_801178EC:

	# ROM: 0x1148EC
	.asciz "std::exception"
	.balign 4
	.asciz "bad_alloc"
	.balign 4

.global lbl_80117908
lbl_80117908:

	# ROM: 0x114908
	.asciz "exception"
	.balign 4
	.4byte 0

.global lbl_80117918
lbl_80117918:

	# ROM: 0x114918
	.asciz "!bad_exception!!"
	.balign 4
	.asciz "!std::exception!!std::bad_exception!!"
	.balign 4
	.asciz "!std::bad_exception!!"
	.balign 4

.global lbl_8011796C
lbl_8011796C:

	# ROM: 0x11496C
	.asciz "std::bad_exception"
	.balign 4

.global lbl_80117980
lbl_80117980:

	# ROM: 0x114980
	.asciz "std::exception"
	.balign 4

.global lbl_80117990
lbl_80117990:

	# ROM: 0x114990
	.asciz "bad_exception"
	.balign 4
	.asciz "exception"
	.balign 4
	.4byte 0

.global lbl_801179B0
lbl_801179B0:

	# ROM: 0x1149B0
	.4byte 0
	.4byte 0
	.4byte 0x41F00000
	.4byte 0
	.4byte 0x41E00000
	.4byte 0

.global lbl_801179C8
lbl_801179C8:

	# ROM: 0x1149C8
	.4byte 0x40240000
	.4byte 0
	.4byte 0x40590000
	.4byte 0
	.4byte 0x40C38800
	.4byte 0
	.4byte 0x4197D784
	.4byte 0
	.4byte 0x4341C379
	.4byte 0x37E08000
	.4byte 0x4693B8B5
	.4byte 0xB5056E17
	.4byte 0x4D384F03
	.4byte 0xE93FF9F5
	.4byte 0x5A827748
	.4byte 0xF9301D32
	.4byte 0x75154FDD
	.4byte 0x7F73BF3C

.global lbl_80117A10
lbl_80117A10:

	# ROM: 0x114A10
	.4byte 0x40240000
	.4byte 0
	.4byte 0x40590000
	.4byte 0
	.4byte 0x408F4000
	.4byte 0
	.4byte 0x40C38800
	.4byte 0
	.4byte 0x40F86A00
	.4byte 0
	.4byte 0x412E8480
	.4byte 0
	.4byte 0x416312D0
	.4byte 0
	.4byte 0x4197D784
	.4byte 0

.global lbl_80117A50
lbl_80117A50:

	# ROM: 0x114A50
	.4byte 0x01010101
	.4byte 0x01010101
	.4byte 0x01020202
	.4byte 0x02020101
	.4byte 0x01010101
	.4byte 0x01010101
	.4byte 0x01010101
	.4byte 0x01010101
	.4byte 0x04080808
	.4byte 0x08080808
	.4byte 0x08080808
	.4byte 0x08080808
	.4byte 0x30303030
	.4byte 0x30303030
	.4byte 0x30300808
	.4byte 0x08080808
	.4byte 0x08A0A0A0
	.4byte 0xA0A0A080
	.4byte 0x80808080
	.4byte 0x80808080
	.4byte 0x80808080
	.4byte 0x80808080
	.4byte 0x80808008
	.4byte 0x08080808
	.4byte 0x08606060
	.4byte 0x60606040
	.4byte 0x40404040
	.4byte 0x40404040
	.4byte 0x40404040
	.4byte 0x40404040
	.4byte 0x40404008
	.4byte 0x08080801
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0

.global lbl_80117B50
lbl_80117B50:

	# ROM: 0x114B50
	.4byte 0x00010203
	.4byte 0x04050607
	.4byte 0x08090A0B
	.4byte 0x0C0D0E0F
	.4byte 0x10111213
	.4byte 0x14151617
	.4byte 0x18191A1B
	.4byte 0x1C1D1E1F
	.4byte 0x20212223
	.4byte 0x24252627
	.4byte 0x28292A2B
	.4byte 0x2C2D2E2F
	.4byte 0x30313233
	.4byte 0x34353637
	.4byte 0x38393A3B
	.4byte 0x3C3D3E3F
	.4byte 0x40616263
	.4byte 0x64656667
	.4byte 0x68696A6B
	.4byte 0x6C6D6E6F
	.4byte 0x70717273
	.4byte 0x74757677
	.4byte 0x78797A5B
	.4byte 0x5C5D5E5F
	.4byte 0x60616263
	.4byte 0x64656667
	.4byte 0x68696A6B
	.4byte 0x6C6D6E6F
	.4byte 0x70717273
	.4byte 0x74757677
	.4byte 0x78797A7B
	.4byte 0x7C7D7E7F
	.4byte 0x80818283
	.4byte 0x84858687
	.4byte 0x88898A8B
	.4byte 0x8C8D8E8F
	.4byte 0x90919293
	.4byte 0x94959697
	.4byte 0x98999A9B
	.4byte 0x9C9D9E9F
	.4byte 0xA0A1A2A3
	.4byte 0xA4A5A6A7
	.4byte 0xA8A9AAAB
	.4byte 0xACADAEAF
	.4byte 0xB0B1B2B3
	.4byte 0xB4B5B6B7
	.4byte 0xB8B9BABB
	.4byte 0xBCBDBEBF
	.4byte 0xC0C1C2C3
	.4byte 0xC4C5C6C7
	.4byte 0xC8C9CACB
	.4byte 0xCCCDCECF
	.4byte 0xD0D1D2D3
	.4byte 0xD4D5D6D7
	.4byte 0xD8D9DADB
	.4byte 0xDCDDDEDF
	.4byte 0xE0E1E2E3
	.4byte 0xE4E5E6E7
	.4byte 0xE8E9EAEB
	.4byte 0xECEDEEEF
	.4byte 0xF0F1F2F3
	.4byte 0xF4F5F6F7
	.4byte 0xF8F9FAFB
	.4byte 0xFCFDFEFF

.global lbl_80117C50
lbl_80117C50:

	# ROM: 0x114C50
	.4byte 0x00010203
	.4byte 0x04050607
	.4byte 0x08090A0B
	.4byte 0x0C0D0E0F
	.4byte 0x10111213
	.4byte 0x14151617
	.4byte 0x18191A1B
	.4byte 0x1C1D1E1F
	.4byte 0x20212223
	.4byte 0x24252627
	.4byte 0x28292A2B
	.4byte 0x2C2D2E2F
	.4byte 0x30313233
	.4byte 0x34353637
	.4byte 0x38393A3B
	.4byte 0x3C3D3E3F
	.4byte 0x40414243
	.4byte 0x44454647
	.4byte 0x48494A4B
	.4byte 0x4C4D4E4F
	.4byte 0x50515253
	.4byte 0x54555657
	.4byte 0x58595A5B
	.4byte 0x5C5D5E5F
	.4byte 0x60414243
	.4byte 0x44454647
	.4byte 0x48494A4B
	.4byte 0x4C4D4E4F
	.4byte 0x50515253
	.4byte 0x54555657
	.4byte 0x58595A7B
	.4byte 0x7C7D7E7F
	.4byte 0x80818283
	.4byte 0x84858687
	.4byte 0x88898A8B
	.4byte 0x8C8D8E8F
	.4byte 0x90919293
	.4byte 0x94959697
	.4byte 0x98999A9B
	.4byte 0x9C9D9E9F
	.4byte 0xA0A1A2A3
	.4byte 0xA4A5A6A7
	.4byte 0xA8A9AAAB
	.4byte 0xACADAEAF
	.4byte 0xB0B1B2B3
	.4byte 0xB4B5B6B7
	.4byte 0xB8B9BABB
	.4byte 0xBCBDBEBF
	.4byte 0xC0C1C2C3
	.4byte 0xC4C5C6C7
	.4byte 0xC8C9CACB
	.4byte 0xCCCDCECF
	.4byte 0xD0D1D2D3
	.4byte 0xD4D5D6D7
	.4byte 0xD8D9DADB
	.4byte 0xDCDDDEDF
	.4byte 0xE0E1E2E3
	.4byte 0xE4E5E6E7
	.4byte 0xE8E9EAEB
	.4byte 0xECEDEEEF
	.4byte 0xF0F1F2F3
	.4byte 0xF4F5F6F7
	.4byte 0xF8F9FAFB
	.4byte 0xFCFDFEFF

.global lbl_80117D50
lbl_80117D50:

	# ROM: 0x114D50
	.4byte 0x002D496E
	.4byte 0x6600496E
	.4byte 0x66004E61
	.4byte 0x4E000000

.global lbl_80117D60
lbl_80117D60:

	# ROM: 0x114D60
	.4byte 0x00A2F983
	.4byte 0x006E4E44
	.4byte 0x001529FC
	.4byte 0x002757D1
	.4byte 0x00F534DD
	.4byte 0x00C0DB62
	.4byte 0x0095993C
	.4byte 0x00439041
	.4byte 0x00FE5163
	.4byte 0x00ABDEBB
	.4byte 0x00C561B7
	.4byte 0x00246E3A
	.4byte 0x00424DD2
	.4byte 0x00E00649
	.4byte 0x002EEA09
	.4byte 0x00D1921C
	.4byte 0x00FE1DEB
	.4byte 0x001CB129
	.4byte 0x00A73EE8
	.4byte 0x008235F5
	.4byte 0x002EBB44
	.4byte 0x0084E99C
	.4byte 0x007026B4
	.4byte 0x005F7E41
	.4byte 0x003991D6
	.4byte 0x00398353
	.4byte 0x0039F49C
	.4byte 0x00845F8B
	.4byte 0x00BDF928
	.4byte 0x003B1FF8
	.4byte 0x0097FFDE
	.4byte 0x0005980F
	.4byte 0x00EF2F11
	.4byte 0x008B5A0A
	.4byte 0x006D1F6D
	.4byte 0x00367ECF
	.4byte 0x0027CB09
	.4byte 0x00B74F46
	.4byte 0x003F669E
	.4byte 0x005FEA2D
	.4byte 0x007527BA
	.4byte 0x00C7EBE5
	.4byte 0x00F17B3D
	.4byte 0x000739F7
	.4byte 0x008A5292
	.4byte 0x00EA6BFB
	.4byte 0x005FB11F
	.4byte 0x008D5D08
	.4byte 0x00560330
	.4byte 0x0046FC7B
	.4byte 0x006BABF0
	.4byte 0x00CFBC20
	.4byte 0x009AF436
	.4byte 0x001DA9E3
	.4byte 0x0091615E
	.4byte 0x00E61B08
	.4byte 0x00659985
	.4byte 0x005F14A0
	.4byte 0x0068408D
	.4byte 0x00FFD880
	.4byte 0x004D7327
	.4byte 0x00310606
	.4byte 0x001556CA
	.4byte 0x0073A8C9
	.4byte 0x0060E27B
	.4byte 0x00C08C6B

.global lbl_80117E68
lbl_80117E68:

	# ROM: 0x114E68
	.4byte 0x3FF921FB
	.4byte 0x400921FB
	.4byte 0x4012D97C
	.4byte 0x401921FB
	.4byte 0x401F6A7A
	.4byte 0x4022D97C
	.4byte 0x4025FDBB
	.4byte 0x402921FB
	.4byte 0x402C463A
	.4byte 0x402F6A7A
	.4byte 0x4031475C
	.4byte 0x4032D97C
	.4byte 0x40346B9C
	.4byte 0x4035FDBB
	.4byte 0x40378FDB
	.4byte 0x403921FB
	.4byte 0x403AB41B
	.4byte 0x403C463A
	.4byte 0x403DD85A
	.4byte 0x403F6A7A
	.4byte 0x40407E4C
	.4byte 0x4041475C
	.4byte 0x4042106C
	.4byte 0x4042D97C
	.4byte 0x4043A28C
	.4byte 0x40446B9C
	.4byte 0x404534AC
	.4byte 0x4045FDBB
	.4byte 0x4046C6CB
	.4byte 0x40478FDB
	.4byte 0x404858EB
	.4byte 0x404921FB

.global lbl_80117EE8
lbl_80117EE8:

	# ROM: 0x114EE8
	.4byte 0x00000002
	.4byte 0x00000003
	.4byte 0x00000004
	.4byte 0x00000006

.global lbl_80117EF8
lbl_80117EF8:

	# ROM: 0x114EF8
	.4byte 0x3FF921FB
	.4byte 0x40000000
	.asciz ">tD-"
	.balign 4
	.4byte 0x3CF84698
	.4byte 0x80000000
	.4byte 0x3B78CC51
	.4byte 0x60000000
	.4byte 0x39F01B83
	.4byte 0x80000000
	.asciz "8z% @"
	.balign 4
	.4byte 0x36E38222
	.4byte 0x80000000
	.4byte 0x3569F31D
	.4byte 0

.global lbl_80117F38
lbl_80117F38:

	# ROM: 0x114F38
	.4byte 0x3FD55555
	.4byte 0x55555563
	.4byte 0x3FC11111
	.4byte 0x1110FE7A
	.4byte 0x3FABA1BA
	.4byte 0x1BB341FE
	.4byte 0x3F9664F4
	.4byte 0x8406D637
	.4byte 0x3F8226E3
	.4byte 0xE96E8493
	.4byte 0x3F6D6D22
	.4byte 0xC9560328
	.4byte 0x3F57DBC8
	.4byte 0xFEE08315
	.4byte 0x3F4344D8
	.4byte 0xF2F26501
	.4byte 0x3F3026F7
	.4byte 0x1A8D1068
	.4byte 0x3F147E88
	.4byte 0xA03792A6
	.4byte 0x3F12B80F
	.4byte 0x32F0A7E9
	.4byte 0xBEF375CB
	.4byte 0xDB605373
	.4byte 0x3EFB2A70
	.4byte 0x74BF7AD4

.global lbl_80117FA0
lbl_80117FA0:

	# ROM: 0x114FA0
	.4byte 0x3FDDAC67
	.4byte 0x0561BB4F
	.4byte 0x3FE921FB
	.4byte 0x54442D18
	.4byte 0x3FEF730B
	.4byte 0xD281F69B
	.4byte 0x3FF921FB
	.4byte 0x54442D18
	.4byte 0x3C7A2B7F
	.4byte 0x222F65E2
	.4byte 0x3C81A626
	.4byte 0x33145C07
	.4byte 0x3C700788
	.4byte 0x7AF0CBBD
	.4byte 0x3C91A626
	.4byte 0x33145C07
	.4byte 0x3FD55555
	.4byte 0x5555550D
	.4byte 0xBFC99999
	.4byte 0x9998EBC4
	.4byte 0x3FC24924
	.4byte 0x920083FF
	.4byte 0xBFBC71C6
	.4byte 0xFE231671
	.4byte 0x3FB745CD
	.4byte 0xC54C206E
	.4byte 0xBFB3B0F2
	.4byte 0xAF749A6D
	.4byte 0x3FB10D66

.global lbl_80118014
lbl_80118014:

	# ROM: 0x115014
	.4byte 0xA0D03D51
	.4byte 0xBFADDE2D
	.4byte 0x52DEFD9A
	.4byte 0x3FA97B4B
	.4byte 0x24760DEB
	.4byte 0xBFA2B444
	.4byte 0x2C6A6C2F
	.4byte 0x3F90AD3A
	.4byte 0xE322DA11

.global lbl_80118038
lbl_80118038:

	# ROM: 0x115038
	.4byte 0x3F800000
	.4byte 0xBEAAAAAA
	.4byte 0x3E4CCC81
	.4byte 0xBE123E7D
	.4byte 0x3DE21F95
	.4byte 0xBDAD417C
	.4byte 0x3D41186D
	.4byte 0x40DA826B
	.asciz "@OYX@"
	.balign 4
	.4byte 0x3FB925AB
	.4byte 0x3F95F61A
	.4byte 0x3F851081
	.4byte 0x36EF692F
	.4byte 0x355C1DF9
	.4byte 0
	.4byte 0x35291D45
	.4byte 0
	.byte 0x00

.global lbl_80118081
lbl_80118081:

	# ROM: 0x115081
	.balign 4
	.4byte 0
	.4byte 0x3EC90EAA
	.4byte 0x3F16CBE4
	.4byte 0x3F490FDA
	.4byte 0x3F7B53C5
	.4byte 0x3F96CBE2
	.4byte 0x3FAFEDD9
	.4byte 0
	.4byte 0x37185D99
	.4byte 0x32C59189
	.4byte 0x33874A9E
	.4byte 0x353CFA83
	.4byte 0x348637BD
	.4byte 0x35541063
	.4byte 0x401A8277
	.4byte 0x3FBF90C7
	.4byte 0x3F800000
	.4byte 0x3F2B0DC1
	.4byte 0x3ED413CD
	.4byte 0x3E4BAFAF
	.4byte 0x3516DC59
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0
	.4byte 0

.global lbl_801180F0
lbl_801180F0:

	# ROM: 0x1150F0
	.4byte 0x3E800000
	.4byte 0x3CBE6080
	.4byte 0x34372200
	.4byte 0x2DA44152
