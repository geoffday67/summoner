symbol LAYOUT_START = 28
symbol END_OF_LAYOUT = $FF
symbol SEGMENT = b1
symbol DISPLAY = b2
	
setfreq m32
	
main:
	bptr = LAYOUT_START
	@bptrinc = %00000000
	@bptrinc = %00010000
	@bptrinc = %00100000
	@bptrinc = %00110000
	@bptrinc = %01000000
	@bptrinc = %01010000
	@bptrinc = %01100000
	@bptr = END_OF_LAYOUT
	bptr = LAYOUT_START

show:
	if @bptr = END_OF_LAYOUT then
		bptr = LAYOUT_START
	endif
	
	b0 = @bptrinc
	SEGMENT = b0 / 16
	'DISPLAY = b0 % 16
	gosub light
	'pause 500
	goto show

light:
	' Light only the specified segment and display
	for b3 = 0 to 7
		if b3 = SEGMENT then
			gosub segment_on
		else
			gosub segment_off
		endif
	next b3
	return
	
segment_on:
	select b3
	case 0 low C.2
	case 1 low C.1
	case 2 low C.0
	case 3 low B.5
	case 4 low B.4
	case 5 low B.3
	case 6 low B.2
	endselect
	return
	
segment_off:
	select b3
	case 0 high C.2
	case 1 high C.1
	case 2 high C.0
	case 3 high B.5
	case 4 high B.4
	case 5 high B.3
	case 6 high B.2
	endselect
	return