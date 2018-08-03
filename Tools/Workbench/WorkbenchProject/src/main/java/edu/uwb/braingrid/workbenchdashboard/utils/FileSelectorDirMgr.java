package edu.uwb.braingrid.workbenchdashboard.utils;

import java.io.File;
import java.util.ArrayList;

public class FileSelectorDirMgr {
	
	ArrayList<File> dirs = new ArrayList<File>();
	File root = new File("C:\\");
	
	public FileSelectorDirMgr() {
	}
	
	
	
	public File getLastDir() {
		if(dirs.isEmpty()) {
			return root;
		}
		return dirs.get(dirs.size() - 1);
	}
	
	public File getDir(int index) {
		if(dirs.isEmpty() || index >= dirs.size()) {
			return root;
		}
		return dirs.get(index);
	}
	
	public void add(File newdir) {
		dirs.add(newdir);
	}
	

}