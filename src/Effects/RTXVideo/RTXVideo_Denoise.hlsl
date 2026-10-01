// NVIDIA VideoSuperRes native backend replaces this placeholder pass.
//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME RTX Video Denoise

//!PARAMETER
//!LABEL Strength
//!DEFAULT 1
//!OPTION 0 Low
//!OPTION 1 Medium
//!OPTION 2 High
//!OPTION 3 Ultra
int strength;

//!TEXTURE
Texture2D INPUT;
//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;
//!SAMPLER
//!FILTER LINEAR
SamplerState sam;
//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT
MF4 Pass1(float2 pos) { return INPUT.SampleLevel(sam, pos, 0); }
