/**
 * @file Theme.kt
 * @brief Jetpack Compose Material 3 Theme for unkXwyno Mirror.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

package best.genzhost.unkxwynomirror.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val BrandPrimary   = Color(0xFF00E5FF)
val BrandSecondary = Color(0xFF7C4DFF)
val BackgroundDark = Color(0xFF0D1117)
val SurfaceDark    = Color(0xFF161B22)
val TextPrimary    = Color(0xFFF0F6FC)
val TextSecondary  = Color(0xFF8B949E)
val StatusGreen    = Color(0xFF00E676)

private val DarkColorScheme = darkColorScheme(
    primary = BrandPrimary,
    secondary = BrandSecondary,
    background = BackgroundDark,
    surface = SurfaceDark,
    onPrimary = Color.Black,
    onSecondary = Color.White,
    onBackground = TextPrimary,
    onSurface = TextPrimary
)

@Composable
fun UnkxwynoTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColorScheme,
        content = content
    )
}
