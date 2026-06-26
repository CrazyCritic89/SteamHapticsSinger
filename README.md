# Steam Haptics Singer (libremidi)
<img align="left" height="90" alt="shs_icon_new" src="https://github.com/user-attachments/assets/0c7629e8-b289-47d1-b0f1-560037f4139e" />
<svg
   viewBox="0 0 36 36"
   fill="none"
   version="1.1"
   id="svg4"
   sodipodi:docname="shs_icon_b.svg"
   inkscape:export-filename="shs_icon.png"
   inkscape:export-xdpi="2730.6667"
   inkscape:export-ydpi="2730.6667"
   inkscape:version="1.4.4 (dcaf3e7d9e, 2026-05-05)"
   xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape"
   xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
   xmlns="http://www.w3.org/2000/svg"
   xmlns:svg="http://www.w3.org/2000/svg">
  <defs
     id="defs4" />
  <g
     id="g10"
     transform="matrix(0.9558466,0,0,0.9558466,0.7947612,0.47193656)"
     style="fill:#ffffff">
    <path
       fill="currentColor"
       d="M 6.68629,12.972521 C 3.79086,15.867951 2,19.867921 2,24.286221 c 0,4.4183 1.79086,8.4183 4.68629,11.3137 l 2.82843,-2.8284 C 7.34315,30.599921 6,27.599921 6,24.286221 c 0,-3.3137 1.34315,-6.3137 3.51472,-8.48527 z"
       id="path1"
       style="fill:#ffffff" />
    <path
       fill="currentColor"
       d="m 26.4853,15.800951 2.8284,-2.82843 c 2.8954,2.89543 4.6863,6.8954 4.6863,11.3137 0,4.4183 -1.7909,8.4183 -4.6863,11.3137 l -2.8284,-2.8284 c 2.1716,-2.1716 3.5147,-5.1716 3.5147,-8.4853 0,-3.3137 -1.3431,-6.3137 -3.5147,-8.48527 z"
       id="path2"
       style="fill:#ffffff" />
    <circle
       fill="currentColor"
       cx="18"
       cy="23.972523"
       r="2"
       id="circle2"
       style="fill:#ffffff" />
    <path
       fill="currentColor"
       d="m 17.8891,15.021171 c -2.2288,0.05461 -4.4412,0.93239 -6.1422,2.63335 -1.81373,1.8138 -2.69151,4.2091 -2.63327,6.5858 h 4.00217 c -0.0597,-1.3525 0.4268,-2.7247 1.4596,-3.7574 0.9199,-0.9199 2.109,-1.4064 3.3137,-1.4596 z"
       id="path3"
       style="fill:#ffffff" />
    <path
       fill="currentColor"
       d="m 23.106,24.240321 h 4.0022 c -0.0546,2.2288 -0.9324,4.4412 -2.6333,6.1421 -1.8138,1.8138 -4.2092,2.6916 -6.5858,2.6333 v -4.0022 c 1.3525,0.0597 2.7246,-0.4268 3.7573,-1.4595 0.9199,-0.9199 1.4064,-2.109 1.4596,-3.3137 z"
       id="path4"
       style="fill:#ffffff" />
    <path
       style="fill:none;stroke:#ffffff;stroke-width:4;stroke-linecap:butt;stroke-linejoin:miter;stroke-miterlimit:4;stroke-opacity:1"
       d="m 18,23.972521 c 0,0 12.841818,-8.831348 -4.049088,-21.2167022 7.389848,4.7708025 11.898585,2.1119279 11.898585,2.1119279"
       id="path5"
       sodipodi:nodetypes="ccc" />
  </g>
</svg>

This branch is a rewrite / enhancement of the playback code (along with other areas). It is specfically moving to libremidi instead of MidiFile. This also attempts to unify real-time MIDI into the main program by using libremidi as well. Currently work in progress.