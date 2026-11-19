package com.automatika.autopilot

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import com.automatika.autopilot.ui.AutopilotScreen
import com.automatika.autopilot.ui.AutopilotViewModel
import com.automatika.autopilot.ui.AutopilotViewModelFactory

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    val vm: AutopilotViewModel = viewModel(
                        factory = AutopilotViewModelFactory(applicationContext),
                    )
                    AutopilotScreen(vm)
                }
            }
        }
    }
}
