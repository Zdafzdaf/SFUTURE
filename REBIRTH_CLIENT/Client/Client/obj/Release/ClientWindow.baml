   M S B A M L   `   `   `  ���� A  =Client, Version=1.0.1.0, Culture=neutral, PublicKeyToken=nullclr-namespace:ClientClient  !clr-namespace:DemoAppDemoApp      Client.ClientWindow   4mah,http://metro.mahapps.com/winfx/xaml/controls  R NWindowsBase, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35W SPresentationCore, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35\ XPresentationFramework, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35D 9http://schemas.microsoft.com/winfx/2006/xaml/presentation    R NSystem.Xaml, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     @d2http://schemas.microsoft.com/expression/blend/2008     Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     Acontrols,http://metro.mahapps.com/winfx/xaml/controls     &localclr-namespace:Client     (exrichclr-namespace:DemoApp     -   $��ThisIsClientWindow��  � Title$*  $Rebirth client. No channel selected.��$	��350��$	��480��$	��641��$	��640������  j� Stretch$
 Fill=� �� ImageSource$( "Resources/LoginPage_Background.png��.��� -    �Name$ MainGrid�� � ColumnDefinitions �� $��* ��� $
��Auto � � RowDefinitions �� $��* ��� $��* ��� $��28 �.+�   DemoApp.RichTextBoxExt  -     Name$ ChatBox��$��0��$��0��$��2��$��1,1,1,1q�$ �� HorizontalScrollBarVisibility$ Visible=�$��Stretch=�" �� VerticalScrollBarVisibility$ Visible=�	 �� 
IsReadOnly	 . $	��200��$	��200��$��14(�
 �� 
LineHeight$
 6�� �� AcceptsReturn .   �� IsUndoEnabled .  &   System.Windows.Media.TextOptions   TextFormattingMode$ Display=�   TextHintingMode$ Fixed=�$��Verdana)� �� IsDocumentEnabled . $
��Auto����Z�    	Emoticons    DemoApp.EmoticonMapper     Text$ :)��   Icon$ Smileys/1.png��  $	 :-c��$ Smileys/101.png��  $	 B-)��$ Smileys/16.png��  $ :D��$ Smileys/4.png��  $ :(��$ Smileys/2.png��. �/� �� -   $��UsersInThisChannel��$��1,1,1,1q�$��1��$��1��$��18(�$��Right=�$	��Top=�$	��156��$	��200��$	��171����Z� � $��2��$��0��$��2��$��Stretch=� �� $��* ��� $
��Auto �.+��� -   $��ChatInputTextBox��$��0��$��1,1,1,1q�
�������$��18(�$	��388����Z� �� $��1��$��Stretch=�$��Stretch=� �� Orientation$ 
Horizontal=�.+��� -   $��1,1,1,1q�
�������$
��Send��$��75��$
��Auto��$��75��$��Stretch=���Z� �� -    ��Name$ CreateGameButton��$��1,1,1,1q�
�������$��Create game��$��85��$��75��$��85��$��Stretch=���Z� �� -   $ GameListButton��$��1,1,1,1q�
�������$��Games��$��75��$
��Auto��$��75��$��Stretch=���Z� 