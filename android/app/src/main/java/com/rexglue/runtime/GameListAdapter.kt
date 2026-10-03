/**
 * GameListAdapter — RecyclerView adapter for the game library grid
 */
package com.rexglue.runtime

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView

/**
 * Adapter displaying a grid of games with cover art, title, and play time.
 */
class GameListAdapter(
    private val onGameClick: (GameEntry) -> Unit,
    private val onGameLongClick: (GameEntry) -> Boolean
) : ListAdapter<GameEntry, GameListAdapter.GameViewHolder>(GameDiffCallback()) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_game, parent, false)
        return GameViewHolder(view)
    }

    override fun onBindViewHolder(holder: GameViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class GameViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val coverImage: ImageView = itemView.findViewById(R.id.game_cover)
        private val titleText: TextView = itemView.findViewById(R.id.game_title)
        private val subtitleText: TextView = itemView.findViewById(R.id.game_subtitle)

        fun bind(game: GameEntry) {
            titleText.text = game.title
            subtitleText.text = if (game.hasBeenPlayed) {
                game.formattedPlayTime
            } else {
                itemView.context.getString(R.string.never_played)
            }

            // Load cover art
            if (game.coverArtUri != null) {
                try {
                    coverImage.setImageURI(game.coverArtUri)
                } catch (_: Exception) {
                    coverImage.setImageResource(R.drawable.ic_game_placeholder)
                }
            } else {
                coverImage.setImageResource(R.drawable.ic_game_placeholder)
            }

            // Click handlers
            itemView.setOnClickListener { onGameClick(game) }
            itemView.setOnLongClickListener { onGameLongClick(game) }
        }
    }

    private class GameDiffCallback : DiffUtil.ItemCallback<GameEntry>() {
        override fun areItemsTheSame(oldItem: GameEntry, newItem: GameEntry): Boolean {
            return oldItem.id == newItem.id
        }

        override fun areContentsTheSame(oldItem: GameEntry, newItem: GameEntry): Boolean {
            return oldItem == newItem
        }
    }
}
