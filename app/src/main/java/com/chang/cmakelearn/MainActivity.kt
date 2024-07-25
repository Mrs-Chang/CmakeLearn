package com.chang.cmakelearn

import android.os.Bundle
import android.util.Log
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.chang.cmakelearn.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.button.setOnClickListener {
            ThreadHook.enableThreadHook()
            Toast.makeText(this@MainActivity, "开启成功", Toast.LENGTH_SHORT).show()
        }

        binding.newthread.setOnClickListener {
            Thread {
                Log.e(
                    "HOOOOOOOOK",
                    "thread name:" + Thread.currentThread().name
                )
                Log.e(
                    "HOOOOOOOOK",
                    "thread id:" + Thread.currentThread().id
                )
                Thread {
                    Log.e(
                        "HOOOOOOOOK",
                        "inner thread name:" + Thread.currentThread().name
                    )
                    Log.e(
                        "HOOOOOOOOK",
                        "inner thread id:" + Thread.currentThread().id
                    )
                }.start()
            }.start()
        }
    }

}