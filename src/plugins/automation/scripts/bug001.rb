# Testovaci skript ze zaslaneho bugreportu.
# Puvodni nazev souboru: 38621A4D913D1E3B-AS30B3PB103X86-130701-161739.INF

def fileRename( item )
  if ( !( item.Name =~ /^\[/ ) and ( item.Name =~ /\.(avi|mkv|mp4)$/ ) )
    nName = item.Name.gsub /^(.*?)(\s(s?\d*[x|e]?\d*\-?\d*))?(\..*)?$/, '[\1]\2\4'
    
    open( ENV['dropbox'] + '\\Backup\\watched.log', 'a') do |f|
      f.puts Salamander.SourcePanel.Path
    end
    
    File.rename( Salamander.SourcePanel.Path+'\\'+item.Name, Salamander.SourcePanel.Path+'\\'+nName )
  end
end

if Salamander.SourcePanel.SelectedItems.count == 0
  fileRename( Salamander.SourcePanel.FocusedItem )
else
  Salamander.SourcePanel.SelectedItems.each { |i| fileRename(i) }
end
