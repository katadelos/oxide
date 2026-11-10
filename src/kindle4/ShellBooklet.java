package org.katadelos.oxide;

import com.amazon.ebook.framework.a.d;
import com.amazon.ebook.framework.a.h;
import com.amazon.ebook.framework.gui.foundation.l;
import com.amazon.ebook.framework.kos.content.j;
import com.amazon.ebook.framework.kos.content.m;
import com.amazon.ebook.framework.kos.content.p;
import com.amazon.ebook.framework.kos.content.r;
import com.amazon.ebook.framework.kos.content.w;
import com.amazon.ebook.framework.kos.content.x;
import java.awt.Container;
import java.awt.EventQueue;
import java.awt.Graphics;
import java.io.*;
import java.net.URI;
import java.util.*;

public final class ShellBooklet extends d implements
        com.amazon.ebook.framework.kos.content.d,
        com.amazon.ebook.framework.kos.content.h {
    private static final j[] TYPES = {new j("sh")};
    private Process process;
    private int generation;

    public String getName() { return "Shell Scripts"; }
    public void create(h context) {
        super.create(context);
        context.getBookletServices().Ul(this);
        context.getBookletServices().cm(this);
        System.out.println("oxide: registered .sh content");
    }
    public Container getMainWindow() { return null; }
    public com.amazon.ebook.framework.gui.event.v getKeyListener() { return null; }
    public void drawPage(l page, Graphics graphics) { }
    public j[] uf() { return TYPES; }
    public j[] Re() { return new j[0]; }

    public boolean qE(URI uri) {
        return uri != null && uri.getPath() != null
            && uri.getPath().toLowerCase(Locale.US).endsWith(".sh");
    }
    private static Process nativeCommand(String action, String path) throws IOException {
        return Runtime.getRuntime().exec(new String[] {
            "/var/local/oxide/oxide", action, path});
    }
    private static void close(Process child) {
        if (child == null) return;
        child.destroy();
        try { child.getInputStream().close(); } catch (IOException ignored) { }
        try { child.getOutputStream().close(); } catch (IOException ignored) { }
        try { child.getErrorStream().close(); } catch (IOException ignored) { }
    }
    public Map gf(String path) {
        Map result = new HashMap();
        Process child = null;
        try {
            child = nativeCommand("_metadata", path);
            BufferedReader reader = new BufferedReader(
                new InputStreamReader(child.getInputStream(), "UTF-8"));
            String name = reader.readLine(), author = reader.readLine();
            if (child.waitFor() != 0 || name == null || author == null)
                throw new IOException("Cannot read script metadata: " + path);
            result.put(m.X, name);
            result.put(m.O, name);
            result.put(m.s, author);
            result.put(m.u, author);
            result.put(m.v, new Long(new File(path).lastModified()));
            result.put(m.W, w.b);
            result.put(m.T, Boolean.FALSE);
            result.put(m.Z, Boolean.FALSE);
        } catch (Exception e) { System.err.println("oxide: " + e); }
        finally { close(child); }
        return result;
    }
    public Object Uf(URI uri, r key) {
        return uri == null ? null : gf(uri.getPath()).get(key);
    }
    public void Oe(URI uri, Map requested) {
        Map metadata = uri == null ? Collections.EMPTY_MAP : gf(uri.getPath());
        for (Iterator it = requested.keySet().iterator(); it.hasNext();) {
            Object key = it.next();
            requested.put(key, metadata.get(key));
        }
    }
    public com.amazon.ebook.framework.gui.widgets.b Wg(p item) {
        com.amazon.ebook.framework.gui.widgets.b row = new com.amazon.ebook.framework.gui.widgets.b();
        Map metadata = gf(item.getFilePath());
        row.KzA((String)metadata.get(m.X));
        row.fAb((String)metadata.get(m.u));
        return row;
    }
    public x lf(URI uri, Map data, String a, String b, String c, int flags) { return null; }

    public synchronized void ZD(URI uri) {
        cancel();
        final int run = generation;
        try {
            if (uri == null || uri.getPath() == null) throw new IOException("Missing script path");
            final String path = uri.getPath();
            // Finish the booklet transition before an app suspends the CVM.
            EventQueue.invokeLater(new Runnable() {
                public void run() {
                    new Thread(new Runnable() {
                        public void run() { execute(path, run); }
                    }, "shell-script").start();
                }
            });
        } catch (IOException e) {
            System.err.println("oxide: " + e);
            returnHome(run);
        }
    }
    private void execute(String path, int run) {
        Process child = null;
        try {
            synchronized (this) {
                if (run != generation) return;
                child = nativeCommand("_launch", path);
                process = child;
            }
            int status = child.waitFor() & 255;
            System.out.println("oxide: exit " + status
                + "; output: /var/local/oxide/last-run.log");
        } catch (Exception e) {
            System.err.println("oxide: " + e);
        } finally {
            close(child);
            synchronized (this) { if (run == generation) process = null; }
            returnHome(run);
        }
    }
    private void returnHome(final int run) {
        EventQueue.invokeLater(new Runnable() {
            public void run() {
                synchronized (ShellBooklet.this) {
                    if (run != generation) return;
                    try { l.getBookletServices().Fl(l.getBookletServices().sL(20, null, true)); }
                    catch (Exception e) { System.err.println("oxide: " + e); }
                }
            }
        });
    }
    private synchronized void cancel() {
        generation++;
        if (process != null) { process.destroy(); process = null; }
    }
    public void stop(com.amazon.ebook.framework.a.a.y event) { cancel(); super.stop(event); }
    public void destroy() { cancel(); super.destroy(); }
}
