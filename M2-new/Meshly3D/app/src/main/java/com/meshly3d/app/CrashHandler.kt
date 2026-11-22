package com.meshly3d.app

import android.content.Context
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter

/** Saves the last Java/Kotlin crash to a file so the next launch can show it on screen. */
object CrashHandler {
    private const val FILE_NAME = "last_crash.txt"

    fun install(context: Context) {
        val appContext = context.applicationContext
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            try {
                val sw = StringWriter()
                error.printStackTrace(PrintWriter(sw))
                File(appContext.filesDir, FILE_NAME).writeText("Thread: ${thread.name}\n$sw")
            } catch (ignored: Throwable) {
            }
            previous?.uncaughtException(thread, error)
        }
    }

    fun read(context: Context): String? {
        val f = File(context.filesDir, FILE_NAME)
        return if (f.exists()) f.readText().take(4000) else null
    }

    fun clear(context: Context) {
        File(context.filesDir, FILE_NAME).delete()
    }
}
