/**
 * GameEntry — Data class representing a detected game
 */
package com.rexglue.runtime

import android.net.Uri

/**
 * Represents a single recompiled game found in the user's game directory.
 *
 * @param id            Unique identifier (derived from module filename)
 * @param title         Display name of the game
 * @param modulePath    Absolute path to the game's .so module
 * @param dataPath      Path to the game's data directory
 * @param coverArtUri   Optional URI to cover art image
 * @param lastPlayed    Timestamp of last play session (0 if never played)
 * @param totalPlayTime Total play time in seconds
 * @param titleId       Xbox 360 title ID (hex string, e.g., "415608C3")
 */
data class GameEntry(
    val id: String,
    val title: String,
    val modulePath: String,
    val dataPath: String,
    val coverArtUri: Uri? = null,
    val lastPlayed: Long = 0L,
    val totalPlayTime: Long = 0L,
    val titleId: String = ""
) {
    /** Whether this game has been played at least once */
    val hasBeenPlayed: Boolean get() = lastPlayed > 0

    /** Formatted play time string */
    val formattedPlayTime: String
        get() {
            val hours = totalPlayTime / 3600
            val minutes = (totalPlayTime % 3600) / 60
            return when {
                hours > 0 -> "${hours}h ${minutes}m"
                minutes > 0 -> "${minutes}m"
                else -> "< 1m"
            }
        }
}
