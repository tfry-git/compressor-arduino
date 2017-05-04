== Very low part count audio compressor based on Arduino ==

Hardware description to follow, but the basic idea is very simple:
    - Connect pin A0 to audio in, biased to 2.5V                                                                                                                                                                                                                               
    - Connect pin D3 to the gate of two 2n7000 N-Fets, back to back, switchin the signal on an off at a high rate